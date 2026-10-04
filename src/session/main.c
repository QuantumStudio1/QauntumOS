#define _GNU_SOURCE
#include "auth.h"
#include "fb.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <openssl/crypto.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/reboot.h>
#include <termios.h>
#include <unistd.h>

#define MAX_INPUTS 32
#define NAV_LEFT 0x101
#define NAV_RIGHT 0x102
#define NAV_UP 0x103
#define NAV_DOWN 0x104
#define SELECT 0x105
#define SWITCH_MODE 0x106
#define FAVORITE 0x107
#define SEARCH 0x108

static int inputs[MAX_INPUTS];
static char input_paths[MAX_INPUTS][128];
static size_t input_count;
static int shift_pressed;
static int desktop_mode;

static void scan_inputs(void) {
    DIR *dir = opendir("/dev/input");
    if (!dir) return;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && input_count < MAX_INPUTS) {
        if (strncmp(entry->d_name, "event", 5) != 0) continue;
        char path[128];
        if (snprintf(path, sizeof(path), "/dev/input/%s", entry->d_name) >= (int)sizeof(path)) continue;
        int known = 0;
        for (size_t i = 0; i < input_count; ++i)
            if (strcmp(input_paths[i], path) == 0) known = 1;
        if (known) continue;
        int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd >= 0) {
            strcpy(input_paths[input_count], path);
            inputs[input_count++] = fd;
        }
    }
    closedir(dir);
}

static int event_char(unsigned code) {
    static const struct { unsigned code; char letter; } letters[] = {
        {KEY_A,'a'}, {KEY_B,'b'}, {KEY_C,'c'}, {KEY_D,'d'}, {KEY_E,'e'},
        {KEY_F,'f'}, {KEY_G,'g'}, {KEY_H,'h'}, {KEY_I,'i'}, {KEY_J,'j'},
        {KEY_K,'k'}, {KEY_L,'l'}, {KEY_M,'m'}, {KEY_N,'n'}, {KEY_O,'o'},
        {KEY_P,'p'}, {KEY_Q,'q'}, {KEY_R,'r'}, {KEY_S,'s'}, {KEY_T,'t'},
        {KEY_U,'u'}, {KEY_V,'v'}, {KEY_W,'w'}, {KEY_X,'x'}, {KEY_Y,'y'},
        {KEY_Z,'z'}
    };
    for (size_t i = 0; i < sizeof(letters) / sizeof(letters[0]); ++i)
        if (code == letters[i].code)
            return shift_pressed ? letters[i].letter - 'a' + 'A' : letters[i].letter;
    if (code >= KEY_1 && code <= KEY_9) return '1' + (int)(code - KEY_1);
    if (code == KEY_0) return '0';
    if (code == KEY_SPACE) return ' ';
    if (code == KEY_MINUS) return '-';
    if (code == KEY_DOT) return '.';
    if (code == KEY_LEFT || code == BTN_DPAD_LEFT) return NAV_LEFT;
    if (code == KEY_RIGHT || code == BTN_DPAD_RIGHT) return NAV_RIGHT;
    if (code == KEY_UP || code == BTN_DPAD_UP) return NAV_UP;
    if (code == KEY_DOWN || code == BTN_DPAD_DOWN) return NAV_DOWN;
    if (code == KEY_TAB || code == BTN_TL || code == BTN_TR) return SWITCH_MODE;
    if (code == KEY_ENTER || code == KEY_KPENTER || code == BTN_START) return '\n';
    if (code == BTN_SOUTH) return SELECT;
    if (code == BTN_NORTH) return FAVORITE;
    if (code == BTN_WEST) return SEARCH;
    if (code == KEY_BACKSPACE || code == BTN_EAST) return '\b';
    return 0;
}

static int next_char(void) {
    struct pollfd fds[MAX_INPUTS + 1];
    for (;;) {
        scan_inputs();
        fds[0] = (struct pollfd){.fd = STDIN_FILENO, .events = POLLIN};
        for (size_t i = 0; i < input_count; ++i)
            fds[i + 1] = (struct pollfd){.fd = inputs[i], .events = POLLIN};
        int ready = poll(fds, input_count + 1, 1000);
        if (ready < 0) { if (errno == EINTR) continue; return -1; }
        if (ready == 0) continue;
        if (fds[0].revents & POLLIN) {
            unsigned char c;
            if (read(STDIN_FILENO, &c, 1) == 1)
                return c == '\t' ? SWITCH_MODE : c;
            return -1;
        }
        if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) return -1;
        for (size_t i = 0; i < input_count; ++i) {
            if (fds[i + 1].revents & (POLLHUP | POLLERR | POLLNVAL)) {
                close(inputs[i]);
                for (size_t j = i + 1; j < input_count; ++j) {
                    inputs[j - 1] = inputs[j];
                    strcpy(input_paths[j - 1], input_paths[j]);
                }
                --input_count;
                break;
            }
            if (!(fds[i + 1].revents & POLLIN)) continue;
            struct input_event event;
            if (read(inputs[i], &event, sizeof(event)) != sizeof(event))
                continue;
            if (event.type == EV_ABS) {
                if (event.code == ABS_HAT0X && event.value < 0) return NAV_LEFT;
                if (event.code == ABS_HAT0X && event.value > 0) return NAV_RIGHT;
                if (event.code == ABS_HAT0Y && event.value < 0) return NAV_UP;
                if (event.code == ABS_HAT0Y && event.value > 0) return NAV_DOWN;
                if (event.code == ABS_X && event.value < -20000) return NAV_LEFT;
                if (event.code == ABS_X && event.value > 20000) return NAV_RIGHT;
                if (event.code == ABS_Y && event.value < -20000) return NAV_UP;
                if (event.code == ABS_Y && event.value > 20000) return NAV_DOWN;
                continue;
            }
            if (event.type != EV_KEY) continue;
            if (event.code == KEY_LEFTSHIFT || event.code == KEY_RIGHTSHIFT) {
                shift_pressed = event.value != 0;
                continue;
            }
            if (event.value == 1) {
                int c = event_char(event.code);
                if (c) return c;
            }
        }
    }
}

static int read_field(const char *stage, const char *profile, const char *prompt,
                      char *buffer, size_t capacity, int secret, const char *message) {
    struct termios original, raw;
    int tty = isatty(STDIN_FILENO) && tcgetattr(STDIN_FILENO, &original) == 0;
    if (tty) {
        raw = original;
        raw.c_lflag &= (tcflag_t)~(ECHO | ICANON);
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    }
    buffer[0] = '\0';
    size_t length = 0;
    int virtual_keyboard = strcmp(stage, "FIRST SETUP") == 0 ||
                           strcmp(stage, "LOCK SCREEN") == 0 ||
                           strcmp(stage, "SEARCH") == 0;
    int home = strcmp(stage, "HOME") == 0 || strcmp(stage, "DESKTOP") == 0;
    int selected = 0;
    printf("\033[2J\033[H\033[1;36m QAUNTUMOS  /  %s\033[0m\n\n", stage);
    if (profile && *profile) printf("  %s\n\n", profile);
    if (message && *message) printf("  %s\n\n", message);
    printf("  %s: ", prompt);
    fflush(stdout);
    qa_ui_render(stage, profile, prompt, buffer, secret, message,
                 virtual_keyboard, selected);
    int result = -1;
    for (;;) {
        int c = next_char();
        if (c < 0) break;
        if (strcmp(stage, "LOCK SCREEN") == 0 && c == SWITCH_MODE) {
            desktop_mode = !desktop_mode;
            qa_ui_set_desktop(desktop_mode);
            qa_ui_render(stage, profile, prompt, buffer, secret, message,
                         virtual_keyboard, selected);
            continue;
        }
        if (virtual_keyboard && c >= NAV_LEFT && c <= NAV_DOWN) {
            int row = selected / 10, column = selected % 10;
            if (c == NAV_LEFT) column = (column + 9) % 10;
            if (c == NAV_RIGHT) column = (column + 1) % 10;
            if (c == NAV_UP) row = (row + 3) % 4;
            if (c == NAV_DOWN) row = (row + 1) % 4;
            selected = row * 10 + column;
            qa_ui_render(stage, profile, prompt, buffer, secret, message,
                         virtual_keyboard, selected);
            continue;
        }
        if (home && c >= NAV_LEFT && c <= NAV_DOWN) {
            if (c == NAV_LEFT || c == NAV_UP) selected = (selected + 4) % 5;
            else selected = (selected + 1) % 5;
            qa_ui_render(stage, profile, prompt, buffer, secret, message,
                         virtual_keyboard, selected);
            continue;
        }
        if (c == SELECT) {
            static const char keys[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-";
            if (home) {
                static const char options[] = "123lp";
                buffer[0] = options[selected]; buffer[1] = '\0';
                result = 0; break;
            }
            c = virtual_keyboard ? (selected == 38 ? '\b' :
                                   selected == 39 ? '\n' : keys[selected]) : '\n';
        }
        if (c == '\r' || c == '\n') { result = 0; break; }
        if (c == '\b' || c == 127) {
            if (length) {
                --length;
                buffer[length] = '\0';
                if (!secret) fputs("\b \b", stdout);
            }
        } else if (c >= 32 && c <= 126 && length + 1 < capacity) {
            buffer[length++] = (char)c;
            buffer[length] = '\0';
            if (!secret) putchar(c);
        }
        fflush(stdout);
        qa_ui_render(stage, profile, prompt, buffer, secret, message,
                     virtual_keyboard, selected);
    }
    if (tty) tcsetattr(STDIN_FILENO, TCSANOW, &original);
    putchar('\n');
    fflush(stdout);
    return result;
}

static int create_profile(const char *directory, int persistent, char *name,
                          char *password, char *confirmation) {
    const char *status = persistent ? "PROFILE SAVED TO DATA DISK" :
                                      "LIVE PROFILE RESETS ON REBOOT";
    for (;;) {
        if (read_field("FIRST SETUP", "CREATE YOUR PROFILE", "PROFILE NAME",
                       name, 64, 0, status) < 0) return -1;
        if (!qa_auth_valid_name(name)) {
            puts("Use 1-32 letters, digits, _ or -.");
            status = "USE LETTERS DIGITS _ OR -";
            continue;
        }
        if (read_field("FIRST SETUP", name, "PASSWORD", password, 160, 1,
                       "AT LEAST 8 CHARACTERS") < 0) return -1;
        if (read_field("FIRST SETUP", name, "CONFIRM PASSWORD", confirmation,
                       160, 1, "KEEP YOUR PASSWORD SAFE") < 0) return -1;
        int valid = strlen(password) >= 8 && strcmp(password, confirmation) == 0;
        OPENSSL_cleanse(confirmation, 160);
        if (!valid) {
            OPENSSL_cleanse(password, 160);
            puts("Passwords differ or are too short.");
            status = "PASSWORDS DIFFER OR TOO SHORT";
            continue;
        }
        int created = qa_auth_create(directory, name, password);
        OPENSSL_cleanse(password, 160);
        if (created == 0) { puts("Profile created. Locking screen."); return 0; }
        perror("Could not create profile");
        status = "NAME TAKEN OR STORAGE ERROR";
    }
}

static int select_profile(const char *directory, char *name) {
    char profiles[16][33];
    int count = qa_auth_list(directory, profiles, 16);
    if (count <= 0) return -1;
    if (count == 1) { strcpy(name, profiles[0]); return 0; }
    int selected = 0;
    for (;;) {
        printf("\033[2J\033[HQAUNTUMOS / CHOOSE PROFILE\n  %s (%d/%d)\n",
               profiles[selected], selected + 1, count);
        fflush(stdout);
        qa_ui_render("CHOOSE PROFILE", profiles[selected], "PRESS A OR ENTER",
                     "", 0, "DPAD CHANGE PROFILE", 0, selected);
        int key = next_char();
        if (key < 0) return -1;
        if (key == '\n' || key == SELECT) {
            strcpy(name, profiles[selected]);
            return 0;
        }
        if (key == NAV_LEFT || key == NAV_UP || key == 'a')
            selected = (selected + count - 1) % count;
        if (key == NAV_RIGHT || key == NAV_DOWN || key == 'd')
            selected = (selected + 1) % count;
    }
}

static int games_menu(const char *profile) {
    const char *directory = getenv("QAUNTUM_LIBRARY_DIR");
    if (!directory || !*directory) directory = "/var/lib/qauntumos/games";
    qa_game games[QA_GAMES_MAX];
    int visible[QA_GAMES_MAX];
    char search[64] = "";
    const char *status = "N ADD GAME  F FAVORITE  I INSTALLED  V FAVORITES";
    int favorites_only = 0, installed_only = 0, selected = 0;
    for (;;) {
        int total = qa_library_load(directory, profile, games, QA_GAMES_MAX);
        if (total < 0) { perror("load game library"); return -1; }
        int count = 0;
        for (int i = 0; i < total; ++i) {
            if (favorites_only && !games[i].favorite) continue;
            if (installed_only && access(games[i].executable, X_OK) != 0) continue;
            if (search[0] && !strcasestr(games[i].title, search)) continue;
            visible[count++] = i;
        }
        if (selected >= count) selected = count ? count - 1 : 0;
        char filter[65];
        snprintf(filter, sizeof(filter), "%d/%d  %s%s%s", count ? selected + 1 : 0,
                 count, favorites_only ? "FAVORITES " : "ALL ",
                 installed_only ? "INSTALLED " : "", search);
        printf("\033[2J\033[HQAUNTUMOS / GAMES (%d/%d)\n", count, total);
        for (int i = 0; i < count; ++i)
            printf("  %c %s%s\n", i == selected ? '>' : ' ',
                   games[visible[i]].favorite ? "* " : "  ",
                   games[visible[i]].title);
        puts("  A/Enter play  F/Y favorite  S/X search  N add  I installed  V favorites  B/Q back");
        fflush(stdout);
        qa_ui_render_library(profile, games, visible, count, selected, filter, status);
        int key = next_char();
        if (key < 0) return -1;
        if (key == NAV_UP || key == NAV_LEFT) {
            if (count) selected = (selected + count - 1) % count;
        } else if (key == NAV_DOWN || key == NAV_RIGHT) {
            if (count) selected = (selected + 1) % count;
        } else if (key == '\b' || key == 'q' || key == 'Q') return 0;
        else if (key == 'v' || key == 'V' || key == SWITCH_MODE) {
            favorites_only = !favorites_only; selected = 0;
        } else if (key == 'i' || key == 'I') {
            installed_only = !installed_only; selected = 0;
        } else if (key == SEARCH || key == 's' || key == 'S') {
            if (read_field("SEARCH", profile, "TITLE CONTAINS", search,
                           sizeof(search), 0, "EMPTY SEARCH SHOWS ALL") < 0) return -1;
            selected = 0;
        } else if (key == 'n' || key == 'N') {
            char title[65], executable[512];
            if (read_field("ADD GAME", profile, "GAME TITLE", title,
                           sizeof(title), 0, "LOCAL GAME") < 0) return -1;
            if (read_field("ADD GAME", profile, "ABSOLUTE EXECUTABLE PATH",
                           executable, sizeof(executable), 0,
                           "KEYBOARD REQUIRED FOR PATH") < 0) return -1;
            size_t length = (size_t)total;
            if (qa_library_add(games, &length, title, executable) < 0 ||
                qa_library_save(directory, profile, games, length) < 0)
                status = "COULD NOT ADD GAME OR PATH IS NOT EXECUTABLE";
            else status = "GAME ADDED";
        } else if ((key == FAVORITE || key == 'f' || key == 'F') && count) {
            games[visible[selected]].favorite = !games[visible[selected]].favorite;
            status = qa_library_save(directory, profile, games, (size_t)total) == 0 ?
                     "FAVORITE UPDATED" : "COULD NOT SAVE FAVORITE";
        } else if ((key == SELECT || key == '\n') && count) {
            status = "STARTING GAME";
            qa_ui_render_library(profile, games, visible, count, selected, filter, status);
            int result = qa_library_launch(&games[visible[selected]]);
            status = result == 0 ? "GAME CLOSED" : "GAME EXITED OR COULD NOT START";
        }
    }
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--check") == 0) {
        puts("qauntum-session build check OK");
        return 0;
    }
    const char *directory = getenv("QAUNTUM_ACCOUNTS_DIR");
    if (!directory || !*directory) directory = "/var/lib/qauntumos/accounts";
    int persistent = getenv("QAUNTUM_PERSISTENT") != NULL;
    scan_inputs();
    char name[64], password[160], confirmation[160], choice[32];
    int count = qa_auth_count(directory);
    if (count < 0) { perror("qauntum-session: accounts"); return 1; }
    if (count == 0 && create_profile(directory, persistent, name,
                                     password, confirmation) < 0) return 1;
    const char *lock_status = "PRESS START TO UNLOCK";
    for (;;) {
        if (select_profile(directory, name) < 0) return 1;
        if (read_field("LOCK SCREEN", name, "PASSWORD", password,
                       sizeof(password), 1, lock_status) < 0) return 1;
        int accepted = qa_auth_verify(directory, name, password);
        OPENSSL_cleanse(password, sizeof(password));
        if (!accepted) {
            puts("Incorrect profile or password.");
            lock_status = "INCORRECT PASSWORD";
            sleep(1);
            continue;
        }
        lock_status = "PRESS START TO UNLOCK";
        printf("Unlocked profile %s.\n", name);
        for (;;) {
            if (read_field(desktop_mode ? "DESKTOP" : "HOME", name,
                           "1 GAMES 2 SETTINGS 3 ADD L LOCK P POWER",
                           choice, sizeof(choice), 0, "CONTROLLER FIRST HOME PREVIEW") < 0) return 1;
            if (choice[0] == 'l' || choice[0] == 'L') break;
            if (choice[0] == 'p' || choice[0] == 'P') { sync(); reboot(RB_POWER_OFF); }
            if (choice[0] == '1') {
                if (games_menu(name) < 0) return 1;
            } else if (choice[0] == '2') {
                char pause[8];
                if (read_field("SETTINGS", name, "PRESS ENTER TO RETURN", pause,
                               sizeof(pause), 0, "QAUNTUMOS ACCOUNT PREVIEW") < 0) return 1;
            } else if (choice[0] == '3') {
                if (create_profile(directory, persistent, name, password,
                                   confirmation) < 0) return 1;
                break;
            }
        }
    }
}
