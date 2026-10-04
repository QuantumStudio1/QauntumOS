#define _GNU_SOURCE
#include "library.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static qa_game games[QA_GAMES_MAX];

static int fail(const char *message) {
    perror(message);
    return 1;
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--check") == 0) {
        puts("qauntum-library build check OK");
        return 0;
    }
    if (argc < 3 || argc > 5) {
        fputs("usage: qauntum-library PROFILE list|add|favorite|launch|search [TITLE] [EXECUTABLE]\n", stderr);
        return 2;
    }
    const char *directory = getenv("QAUNTUM_LIBRARY_DIR");
    if (!directory || !*directory) directory = "/var/lib/qauntumos/games";
    int count = qa_library_load(directory, argv[1], games, QA_GAMES_MAX);
    if (count < 0) return fail("load library");
    if (strcmp(argv[2], "list") == 0 && argc == 3) {
        for (int i = 0; i < count; ++i)
            printf("%s\t%s\t%s\n", games[i].favorite ? "*" : " ",
                   games[i].title, games[i].executable);
        return 0;
    }
    if (strcmp(argv[2], "search") == 0 && argc == 4) {
        for (int i = 0; i < count; ++i)
            if (strcasestr(games[i].title, argv[3]))
                printf("%s\t%s\n", games[i].favorite ? "*" : " ", games[i].title);
        return 0;
    }
    if (strcmp(argv[2], "add") == 0 && argc == 5) {
        size_t length = (size_t)count;
        if (qa_library_add(games, &length, argv[3], argv[4]) < 0)
            return fail("add game");
        return qa_library_save(directory, argv[1], games, length) < 0 ?
            fail("save library") : 0;
    }
    if (argc == 4 && (strcmp(argv[2], "favorite") == 0 ||
                      strcmp(argv[2], "launch") == 0)) {
        for (int i = 0; i < count; ++i) {
            if (strcmp(games[i].title, argv[3]) != 0) continue;
            if (strcmp(argv[2], "launch") == 0) {
                int result = qa_library_launch(&games[i]);
                return result < 0 ? fail("launch game") : result;
            }
            games[i].favorite = !games[i].favorite;
            return qa_library_save(directory, argv[1], games, (size_t)count) < 0 ?
                fail("save library") : 0;
        }
        fputs("game not found\n", stderr);
        return 1;
    }
    fputs("invalid command\n", stderr);
    return 2;
}
