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

static int inputs[MAX_INPUTS];
static size_t input_count;
static int shift_pressed;

static void scan_inputs(void) {
    DIR *dir = opendir("/dev/input");
    if (!dir) return;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && input_count < MAX_INPUTS) {
        if (strncmp(entry->d_name, "event", 5) != 0) continue;
        char path[128];
        if (snprintf(path, sizeof(path), "/dev/input/%s", entry->d_name) >= (int)sizeof(path)) continue;
        int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd >= 0) inputs[input_count++] = fd;
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
    if (code == KEY_ENTER || code == KEY_KPENTER || code == BTN_START) return '\n';
    if (code == BTN_SOUTH) return SELECT;
    if (code == KEY_BACKSPACE || code == BTN_EAST) return '\b';
    return 0;
}

static int next_char(void) {
    struct pollfd fds[MAX_INPUTS + 1];
    fds[0] = (struct pollfd){.fd = STDIN_FILENO, .events = POLLIN};
    for (size_t i = 0; i < input_count; ++i)
        fds[i + 1] = (struct pollfd){.fd = inputs[i], .events = POLLIN};
    for (;;) {
        int ready = poll(fds, input_count + 1, -1);
        if (ready < 0) { if (errno == EINTR) continue; return -1; }
        if (fds[0].revents & POLLIN) {
            unsigned char c;
            if (read(STDIN_FILENO, &c, 1) == 1) return c;
            return -1;
        }
        if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) return -1;
        for (size_t i = 0; i < input_count; ++i) {
            if (!(fds[i + 1].revents & POLLIN)) continue;
            struct input_event event;
            if (read(inputs[i], &event, sizeof(event)) != sizeof(event))
                continue;
            if (event.type == EV_ABS) {
                if (event.code == ABS_HAT0X && event.value < 0) return NAV_LEFT;
                if (event.code == ABS_HAT0X && event.value > 0) return NAV_RIGHT;
                if (event.code == ABS_HAT0Y && event.value < 0) return NAV_UP;
                if (event.code == ABS_HAT0Y && event.value > 0) return NAV_DOWN;
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
                           strcmp(stage, "LOCK SCREEN") == 0;
    int home = strcmp(stage, "HOME") == 0;
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
            if (read_field("HOME", name, "1 GAMES 2 SETTINGS 3 ADD L LOCK P POWER",
                           choice, sizeof(choice), 0, "CONTROLLER FIRST HOME PREVIEW") < 0) return 1;
            if (choice[0] == 'l' || choice[0] == 'L') break;
            if (choice[0] == 'p' || choice[0] == 'P') { sync(); reboot(RB_POWER_OFF); }
            if (choice[0] == '1') {
                char pause[8];
                if (read_field("GAMES", name, "PRESS ENTER TO RETURN", pause,
                               sizeof(pause), 0, "YOUR LIBRARY IS EMPTY") < 0) return 1;
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
