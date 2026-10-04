#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/reboot.h>
#include <sys/utsname.h>
#include <unistd.h>

static void list_directory(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) {
        fprintf(stderr, "ls: %s: %s\n", path, strerror(errno));
        return;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, ".."))
            puts(entry->d_name);
    }
    closedir(dir);
}

static void print_file(const char *path) {
    if (!path) {
        fputs("cat: missing file name\n", stderr);
        return;
    }
    FILE *file = fopen(path, "r");
    if (!file) {
        fprintf(stderr, "cat: %s: %s\n", path, strerror(errno));
        return;
    }
    char buffer[4096];
    size_t count;
    while ((count = fread(buffer, 1, sizeof(buffer), file)) > 0)
        fwrite(buffer, 1, count, stdout);
    if (ferror(file)) fprintf(stderr, "cat: %s: read error\n", path);
    fclose(file);
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--check") == 0) {
        puts("qauntum-shell build check OK");
        return 0;
    }
    puts("QauntumOS boot ready");
    puts("Type 'help' for commands.");
    char *line = NULL;
    size_t capacity = 0;
    for (;;) {
        fputs("qauntum> ", stdout);
        fflush(stdout);
        if (getline(&line, &capacity, stdin) < 0) break;
        char *save = NULL;
        char *command = strtok_r(line, " \t\r\n", &save);
        if (!command) continue;
        char *argument = strtok_r(NULL, " \t\r\n", &save);
        if (!strcmp(command, "help"))
            puts("help version uname ls [dir] cat <file> echo [text] reboot poweroff exit");
        else if (!strcmp(command, "version"))
            puts("QauntumOS Version 2 development preview");
        else if (!strcmp(command, "uname")) {
            struct utsname info;
            if (uname(&info) == 0) printf("Linux %s %s\n", info.release, info.machine);
        } else if (!strcmp(command, "ls")) list_directory(argument ? argument : ".");
        else if (!strcmp(command, "cat")) print_file(argument);
        else if (!strcmp(command, "echo")) puts(argument ? argument : "");
        else if (!strcmp(command, "reboot")) { sync(); reboot(RB_AUTOBOOT); }
        else if (!strcmp(command, "poweroff")) { sync(); reboot(RB_POWER_OFF); }
        else if (!strcmp(command, "exit")) break;
        else fprintf(stderr, "Unknown command: %s\n", command);
    }
    free(line);
    return 0;
}
