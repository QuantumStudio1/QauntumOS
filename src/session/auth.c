#define _GNU_SOURCE
#include "auth.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define SALT_BYTES 16
#define HASH_BYTES 32
#define ITERATIONS 200000

int qa_auth_valid_name(const char *name) {
    if (!name) return 0;
    size_t length = strlen(name);
    if (length < 1 || length > 32) return 0;
    for (size_t i = 0; i < length; ++i) {
        char c = name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-')) return 0;
    }
    return 1;
}

static int ensure_directory(const char *path) {
    char *copy = strdup(path);
    if (!copy) return -1;
    for (char *p = copy + 1; *p; ++p) {
        if (*p != '/') continue;
        *p = '\0';
        if (mkdir(copy, 0700) < 0 && errno != EEXIST) {
            free(copy);
            return -1;
        }
        *p = '/';
    }
    int result = mkdir(copy, 0700);
    if (result < 0 && errno == EEXIST) result = 0;
    free(copy);
    return result;
}

static char *account_path(const char *directory, const char *name) {
    char *path = NULL;
    if (asprintf(&path, "%s/%s", directory, name) < 0) return NULL;
    return path;
}

static void hex_encode(const unsigned char *bytes, size_t length, char *out) {
    static const char alphabet[] = "0123456789abcdef";
    for (size_t i = 0; i < length; ++i) {
        out[i * 2] = alphabet[bytes[i] >> 4];
        out[i * 2 + 1] = alphabet[bytes[i] & 15];
    }
    out[length * 2] = '\0';
}

static int nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static int hex_decode(const char *text, size_t length, unsigned char *out) {
    for (size_t i = 0; i < length; ++i) {
        int hi = nibble(text[i * 2]);
        int lo = nibble(text[i * 2 + 1]);
        if (hi < 0 || lo < 0) return -1;
        out[i] = (unsigned char)((hi << 4) | lo);
    }
    return 0;
}

int qa_auth_count(const char *directory) {
    DIR *dir = opendir(directory);
    if (!dir) return errno == ENOENT ? 0 : -1;
    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
        if (qa_auth_valid_name(entry->d_name)) ++count;
    closedir(dir);
    return count;
}

static int compare_names(const void *left, const void *right) {
    return strcmp((const char *)left, (const char *)right);
}

int qa_auth_list(const char *directory, char names[][33], size_t capacity) {
    DIR *dir = opendir(directory);
    if (!dir) return errno == ENOENT ? 0 : -1;
    size_t count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && count < capacity) {
        if (!qa_auth_valid_name(entry->d_name)) continue;
        struct stat info;
        if (fstatat(dirfd(dir), entry->d_name, &info, AT_SYMLINK_NOFOLLOW) < 0 ||
            !S_ISREG(info.st_mode)) continue;
        strcpy(names[count++], entry->d_name);
    }
    closedir(dir);
    qsort(names, count, sizeof(names[0]), compare_names);
    return (int)count;
}

int qa_auth_create(const char *directory, const char *name, const char *password) {
    if (!qa_auth_valid_name(name) || !password || strlen(password) < 8 ||
        strlen(password) > 128) { errno = EINVAL; return -1; }
    if (ensure_directory(directory) < 0) return -1;
    char *path = account_path(directory, name);
    if (!path) return -1;
    if (access(path, F_OK) == 0) { free(path); errno = EEXIST; return -1; }

    unsigned char salt[SALT_BYTES], hash[HASH_BYTES];
    if (RAND_bytes(salt, sizeof(salt)) != 1 ||
        PKCS5_PBKDF2_HMAC(password, (int)strlen(password), salt, sizeof(salt),
                          ITERATIONS, EVP_sha256(), sizeof(hash), hash) != 1) {
        free(path); errno = EIO; return -1;
    }
    char salt_hex[SALT_BYTES * 2 + 1], hash_hex[HASH_BYTES * 2 + 1];
    hex_encode(salt, sizeof(salt), salt_hex);
    hex_encode(hash, sizeof(hash), hash_hex);
    OPENSSL_cleanse(hash, sizeof(hash));

    char *temporary = NULL;
    if (asprintf(&temporary, "%s/.new-XXXXXX", directory) < 0) {
        free(path); return -1;
    }
    int fd = mkstemp(temporary);
    if (fd < 0) { free(path); free(temporary); return -1; }
    int result = -1;
    FILE *file = fdopen(fd, "w");
    if (!file) close(fd);
    else {
        int wrote = fchmod(fd, 0600) == 0 &&
            fprintf(file, "pbkdf2-sha256:%d:%s:%s\n", ITERATIONS, salt_hex, hash_hex) > 0 &&
            fflush(file) == 0 && fsync(fd) == 0;
        int closed = fclose(file) == 0;
        if (wrote && closed && link(temporary, path) == 0) result = 0;
    }
    int saved = errno;
    unlink(temporary);
    free(temporary);
    free(path);
    errno = saved;
    return result;
}

int qa_auth_verify(const char *directory, const char *name, const char *password) {
    if (!qa_auth_valid_name(name) || !password) return 0;
    char *path = account_path(directory, name);
    if (!path) return 0;
    int fd = open(path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    free(path);
    if (fd < 0) return 0;
    char record[160];
    ssize_t count = read(fd, record, sizeof(record) - 1);
    close(fd);
    if (count <= 0 || count >= (ssize_t)sizeof(record) - 1) return 0;
    record[count] = '\0';
    if (record[count - 1] != '\n') return 0;
    record[count - 1] = '\0';
    int iterations = 0, consumed = 0;
    char salt_hex[SALT_BYTES * 2 + 1], hash_hex[HASH_BYTES * 2 + 1];
    if (sscanf(record, "pbkdf2-sha256:%d:%32[0-9a-f]:%64[0-9a-f]%n",
               &iterations, salt_hex, hash_hex, &consumed) != 3 ||
        consumed != count - 1 ||
        iterations != ITERATIONS || strlen(salt_hex) != SALT_BYTES * 2 ||
        strlen(hash_hex) != HASH_BYTES * 2) return 0;
    unsigned char salt[SALT_BYTES], expected[HASH_BYTES], actual[HASH_BYTES];
    if (hex_decode(salt_hex, sizeof(salt), salt) < 0 ||
        hex_decode(hash_hex, sizeof(expected), expected) < 0 ||
        PKCS5_PBKDF2_HMAC(password, (int)strlen(password), salt, sizeof(salt),
                          ITERATIONS, EVP_sha256(), sizeof(actual), actual) != 1) return 0;
    int match = CRYPTO_memcmp(expected, actual, sizeof(actual)) == 0;
    OPENSSL_cleanse(actual, sizeof(actual));
    return match;
}
