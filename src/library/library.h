#ifndef QAUNTUM_LIBRARY_H
#define QAUNTUM_LIBRARY_H

#include <stddef.h>

#define QA_GAMES_MAX 64

typedef struct {
    char title[65];
    char executable[512];
    int favorite;
} qa_game;

int qa_library_load(const char *directory, const char *profile,
                    qa_game *games, size_t capacity);
int qa_library_save(const char *directory, const char *profile,
                    const qa_game *games, size_t count);
int qa_library_add(qa_game *games, size_t *count, const char *title,
                   const char *executable);
int qa_library_launch(const qa_game *game);
int qa_library_valid_profile(const char *profile);

#endif
