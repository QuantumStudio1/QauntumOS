#ifndef QAUNTUM_FB_H
#define QAUNTUM_FB_H

#include "../library/library.h"

void qa_ui_set_desktop(int enabled);
void qa_ui_render_library(const char *profile, const qa_game *games,
                          const int *visible, int count, int selected,
                          const char *filter, const char *status);

void qa_ui_render(const char *stage, const char *profile, const char *prompt,
                  const char *input, int masked, const char *message,
                  int virtual_keyboard, int selected);

#endif
