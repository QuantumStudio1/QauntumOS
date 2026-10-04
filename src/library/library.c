#define _GNU_SOURCE
#include "library.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

int qa_library_valid_profile(const char *profile) {
    if (!profile) return 0;
    size_t length = strlen(profile);
    if (length < 1 || length > 32) return 0;
    for (size_t i = 0; i < length; ++i) {
        char c = profile[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-')) return 0;
    }
    return 1;
}

static int valid_title(const char *title) {
    if (!title) return 0;
    size_t length = strlen(title);
    if (length < 1 || length > 64) return 0;
    for (size_t i = 0; i < length; ++i)
        if ((unsigned char)title[i] < 32 || title[i] == 127 || title[i] == '\t') return 0;
    return 1;
}

static int valid_path(const char *path) {
    if (!path || path[0] != '/' || strlen(path) >= 512) return 0;
    if (strchr(path, '\t') || strchr(path, '\n') || strchr(path, '\r')) return 0;
    return 1;
}

static int valid_executable(const char *path) {
    if (!valid_path(path)) return 0;
    struct stat info;
    return stat(path, &info) == 0 && S_ISREG(info.st_mode) && access(path, X_OK) == 0;
}

static int make_path(char *out, size_t capacity, const char *directory,
                     const char *profile) {
    if (!directory || !qa_library_valid_profile(profile)) { errno = EINVAL; return -1; }
    int length = snprintf(out, capacity, "%s/%s.games", directory, profile);
    if (length < 0 || (size_t)length >= capacity) { errno = ENAMETOOLONG; return -1; }
    return 0;
}

int qa_library_load(const char *directory, const char *profile,
                    qa_game *games, size_t capacity) {
    char path[1024];
    if (make_path(path, sizeof(path), directory, profile) < 0) return -1;
    int fd = open(path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return errno == ENOENT ? 0 : -1;
    FILE *file = fdopen(fd, "r");
    if (!file) { close(fd); return -1; }
    char *line = NULL;
    size_t length = 0, count = 0;
    int result = -1;
    while (getline(&line, &length, file) >= 0) {
        if (count >= capacity) { errno = E2BIG; goto done; }
        char *first = strchr(line, '\t');
        char *second = first ? strchr(first + 1, '\t') : NULL;
        char *end = second ? strchr(second + 1, '\n') : NULL;
        if (!first || !second || !end || end[1] != '\0') { errno = EINVAL; goto done; }
        *first = '\0'; *second = '\0'; *end = '\0';
        if (!valid_title(line) || strlen(first + 1) >= sizeof(games[count].executable) ||
            (strcmp(second + 1, "0") && strcmp(second + 1, "1"))) {
            errno = EINVAL; goto done;
        }
        strcpy(games[count].title, line);
        strcpy(games[count].executable, first + 1);
        games[count].favorite = second[1] == '1';
        ++count;
    }
    result = ferror(file) ? -1 : (int)count;
done:
    free(line);
    fclose(file);
    return result;
}

int qa_library_save(const char *directory, const char *profile,
                    const qa_game *games, size_t count) {
    char path[1024], temporary[1040];
    if (count > QA_GAMES_MAX || make_path(path, sizeof(path), directory, profile) < 0)
        return -1;
    if (mkdir(directory, 0700) < 0 && errno != EEXIST) return -1;
    if (snprintf(temporary, sizeof(temporary), "%s/.games-XXXXXX", directory) >=
        (int)sizeof(temporary)) { errno = ENAMETOOLONG; return -1; }
    int fd = mkstemp(temporary);
    if (fd < 0) return -1;
    FILE *file = fdopen(fd, "w");
    if (!file) { close(fd); unlink(temporary); return -1; }
    int ok = fchmod(fd, 0600) == 0;
    for (size_t i = 0; i < count; ++i) {
        if (!valid_title(games[i].title) || !valid_path(games[i].executable) ||
            fprintf(file, "%s\t%s\t%d\n", games[i].title, games[i].executable,
                    games[i].favorite != 0) < 0) { ok = 0; break; }
    }
    if (fflush(file) < 0 || fsync(fd) < 0) ok = 0;
    if (fclose(file) < 0) ok = 0;
    if (ok && rename(temporary, path) < 0) ok = 0;
    int saved = errno;
    unlink(temporary);
    errno = saved;
    return ok ? 0 : -1;
}

int qa_library_add(qa_game *games, size_t *count, const char *title,
                   const char *executable) {
    if (!games || !count || *count >= QA_GAMES_MAX || !valid_title(title) ||
        !valid_executable(executable)) { errno = EINVAL; return -1; }
    for (size_t i = 0; i < *count; ++i)
        if (strcmp(games[i].title, title) == 0) { errno = EEXIST; return -1; }
    qa_game *game = &games[(*count)++];
    strcpy(game->title, title);
    strcpy(game->executable, executable);
    game->favorite = 0;
    return 0;
}

int qa_library_launch(const qa_game *game) {
    if (!game || !valid_executable(game->executable)) { errno = EINVAL; return -1; }
    /* The live prototype session is root until real Unix users exist. */
    if (geteuid() == 0) { errno = EPERM; return -1; }
    pid_t child = fork();
    if (child < 0) return -1;
    if (child == 0) {
        char *const argv[] = {(char *)game->executable, NULL};
        execv(game->executable, argv);
        _exit(127);
    }
    int status;
    while (waitpid(child, &status, 0) < 0) {
        if (errno == EINTR) continue;
        return -1;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128;
}
