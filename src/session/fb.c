#define _GNU_SOURCE
#include "fb.h"

#include <ctype.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/kd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

static int fb_fd = -2;
static unsigned char *pixels;
static size_t pixels_size;
static struct fb_fix_screeninfo fixed;
static struct fb_var_screeninfo variable;
static int desktop_mode;

void qa_ui_set_desktop(int enabled) { desktop_mode = enabled != 0; }

static void open_framebuffer(void) {
    if (fb_fd != -2) return;
    fb_fd = open("/dev/fb0", O_RDWR | O_CLOEXEC);
    if (fb_fd < 0) return;
    if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &fixed) < 0 ||
        ioctl(fb_fd, FBIOGET_VSCREENINFO, &variable) < 0 ||
        (variable.bits_per_pixel != 16 && variable.bits_per_pixel != 24 &&
         variable.bits_per_pixel != 32)) {
        close(fb_fd); fb_fd = -1; return;
    }
    pixels_size = fixed.smem_len;
    pixels = mmap(NULL, pixels_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (pixels == MAP_FAILED) { pixels = NULL; close(fb_fd); fb_fd = -1; }
    else {
        int tty = open("/dev/tty0", O_RDWR | O_CLOEXEC);
        if (tty >= 0) { (void)ioctl(tty, KDSETMODE, KD_GRAPHICS); close(tty); }
        fprintf(stderr, "qauntum-session: framebuffer %ux%u active\n",
                variable.xres, variable.yres);
    }
}

static uint32_t channel(unsigned char value, struct fb_bitfield field) {
    if (!field.length) return 0;
    uint32_t maximum = (1u << field.length) - 1u;
    return ((uint32_t)value * maximum / 255u) << field.offset;
}

static void point(int x, int y, uint32_t color) {
    if (x < 0 || y < 0 || x >= (int)variable.xres || y >= (int)variable.yres) return;
    unsigned bpp = variable.bits_per_pixel / 8;
    size_t offset = (size_t)(y + variable.yoffset) * fixed.line_length +
                    (size_t)(x + variable.xoffset) * bpp;
    if (offset + bpp > pixels_size) return;
    for (unsigned i = 0; i < bpp; ++i) pixels[offset + i] = (unsigned char)(color >> (8 * i));
}

static uint32_t rgb(unsigned char red, unsigned char green, unsigned char blue) {
    return channel(red, variable.red) | channel(green, variable.green) |
           channel(blue, variable.blue);
}

static void rectangle(int x, int y, int width, int height, uint32_t color) {
    for (int yy = y; yy < y + height; ++yy)
        for (int xx = x; xx < x + width; ++xx) point(xx, yy, color);
}

static void disc(int cx, int cy, int radius, uint32_t color) {
    for (int y = -radius; y <= radius; ++y)
        for (int x = -radius; x <= radius; ++x)
            if (x * x + y * y <= radius * radius) point(cx + x, cy + y, color);
}

static const unsigned char *glyph(char c) {
    static const unsigned char letters[26][7] = {
        {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30},
        {14,17,16,16,16,17,14}, {30,17,17,17,17,17,30},
        {31,16,16,30,16,16,31}, {31,16,16,30,16,16,16},
        {14,17,16,23,17,17,15}, {17,17,17,31,17,17,17},
        {31,4,4,4,4,4,31}, {7,2,2,2,18,18,12},
        {17,18,20,24,20,18,17}, {16,16,16,16,16,16,31},
        {17,27,21,21,17,17,17}, {17,25,21,19,17,17,17},
        {14,17,17,17,17,17,14}, {30,17,17,30,16,16,16},
        {14,17,17,17,21,18,13}, {30,17,17,30,20,18,17},
        {15,16,16,14,1,1,30}, {31,4,4,4,4,4,4},
        {17,17,17,17,17,17,14}, {17,17,17,17,17,10,4},
        {17,17,17,21,21,21,10}, {17,17,10,4,10,17,17},
        {17,17,10,4,4,4,4}, {31,1,2,4,8,16,31}
    };
    static const unsigned char digits[10][7] = {
        {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14},
        {14,17,1,2,4,8,31}, {30,1,1,14,1,1,30},
        {2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
        {14,16,16,30,17,17,14}, {31,1,2,4,8,8,8},
        {14,17,17,14,17,17,14}, {14,17,17,15,1,1,14}
    };
    static const unsigned char space[7] = {0};
    static const unsigned char dash[7] = {0,0,0,31,0,0,0};
    static const unsigned char underscore[7] = {0,0,0,0,0,0,31};
    static const unsigned char colon[7] = {0,4,4,0,4,4,0};
    static const unsigned char dot[7] = {0,0,0,0,0,12,12};
    static const unsigned char slash[7] = {1,1,2,4,8,16,16};
    static const unsigned char star[7] = {0,21,14,31,14,21,0};
    static const unsigned char question[7] = {14,17,1,2,4,0,4};
    static const unsigned char left[7] = {2,4,8,16,8,4,2};
    static const unsigned char right[7] = {8,4,2,1,2,4,8};
    c = (char)toupper((unsigned char)c);
    if (c >= 'A' && c <= 'Z') return letters[c - 'A'];
    if (c >= '0' && c <= '9') return digits[c - '0'];
    if (c == '-') return dash;
    if (c == '_') return underscore;
    if (c == ':') return colon;
    if (c == '.') return dot;
    if (c == '/') return slash;
    if (c == '*') return star;
    if (c == '?') return question;
    if (c == '<') return left;
    if (c == '>') return right;
    return space;
}

static void draw_text(int x, int y, int scale, const char *text, uint32_t color) {
    if (!text) return;
    for (size_t i = 0; text[i] && i < 64; ++i) {
        const unsigned char *bitmap = glyph(text[i]);
        for (int row = 0; row < 7; ++row)
            for (int col = 0; col < 5; ++col)
                if (bitmap[row] & (1 << (4 - col)))
                    rectangle(x + (int)i * scale * 6 + col * scale,
                              y + row * scale, scale, scale, color);
    }
}

void qa_ui_render(const char *stage, const char *profile, const char *prompt,
                  const char *input, int masked, const char *message,
                  int virtual_keyboard, int selected) {
    open_framebuffer();
    if (!pixels) return;
    int w = (int)variable.xres, h = (int)variable.yres;
    for (int y = 0; y < h; ++y) {
        unsigned char shade = (unsigned char)(12 + y * 22 / (h ? h : 1));
        rectangle(0, y, w, 1, rgb(6, shade, (unsigned char)(29 + y * 29 / (h ? h : 1))));
    }
    int scale = w >= 1000 ? 4 : 3;
    int margin = w / 12;
    int panel_y = h / 3;
    /* Layered halos and bands remain legible without GPU acceleration. */
    disc(w - margin - 80, h / 3, h / 3, rgb(14, 44, 79));
    disc(w - margin - 60, h / 3, h / 4, rgb(14, 55, 92));
    disc(w - margin - 45, h / 3, h / 6, rgb(18, 70, 100));
    rectangle(0, 0, w, 5, rgb(61, 214, 219));
    rectangle(margin, h / 9, 12, 54, rgb(64, 226, 214));
    draw_text(margin + 30, h / 9 + 2, scale + 1, "QAUNTUMOS", rgb(246, 249, 255));
    draw_text(margin + 30, h / 9 + 61, 2, stage, rgb(91, 223, 222));

    rectangle(margin + 6, panel_y + 10, w - 2 * margin, h / 2, rgb(10, 29, 49));
    rectangle(margin, panel_y, w - 2 * margin, h / 2, rgb(24, 42, 70));
    rectangle(margin, panel_y, w - 2 * margin, 3, rgb(77, 122, 159));
    rectangle(margin, panel_y, 5, h / 2, rgb(58, 219, 203));
    if (strcmp(stage, "LOCK SCREEN") == 0) {
        int cx = w - margin - 100, cy = panel_y + 72;
        disc(cx, cy, 51, rgb(50, 125, 171));
        disc(cx, cy, 46, rgb(22, 67, 103));
        disc(cx, cy, 40, rgb(29, 84, 115));
        char initial[2] = {(profile && *profile) ? profile[0] : 'Q', '\0'};
        draw_text(cx - 11, cy - 17, 5, initial, rgb(240, 255, 255));
        int mode_y = panel_y + 69;
        draw_text(margin + 36, mode_y, 2, "CHOOSE YOUR SPACE", rgb(158, 189, 216));
        for (int i = 0; i < 2; ++i) {
            int x = margin + 36 + i * 164;
            int active = desktop_mode == i;
            rectangle(x, mode_y + 28, 154, 43,
                      active ? rgb(61, 219, 206) : rgb(12, 31, 52));
            draw_text(x + 13, mode_y + 42, 2, i ? "DESKTOP" : "CONSOLE",
                      active ? rgb(5, 27, 40) : rgb(195, 213, 230));
        }
    }
    draw_text(margin + 36, panel_y + 25, scale, profile, rgb(255, 255, 255));
    if (strcmp(stage, "LOCK SCREEN") != 0)
        draw_text(margin + 36, panel_y + 94, 2, prompt, rgb(185, 206, 224));
    else draw_text(margin + 36, panel_y + 122, 2, prompt, rgb(185, 206, 224));
    rectangle(margin + 36, panel_y + 145, w - 2 * margin - 72, 56, rgb(11, 25, 43));
    rectangle(margin + 36, panel_y + 145, w - 2 * margin - 72, 2, rgb(73, 126, 155));
    if (masked) {
        char stars[65];
        size_t length = input ? strlen(input) : 0;
        if (length > 64) length = 64;
        memset(stars, '*', length);
        stars[length] = '\0';
        draw_text(margin + 50, panel_y + 160, 2, stars, rgb(255, 255, 255));
    } else draw_text(margin + 50, panel_y + 160, 2, input, rgb(255, 255, 255));
    if (virtual_keyboard) {
        static const char keys[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-";
        int left = margin + 36;
        int cell_width = (w - 2 * margin - 72) / 10;
        for (int i = 0; i < 40; ++i) {
            int x = left + (i % 10) * cell_width;
            int y = panel_y + 215 + (i / 10) * 25;
            rectangle(x + 1, y, cell_width - 4, 22,
                      i == selected ? rgb(45, 193, 169) : rgb(12, 28, 46));
            char key[2] = {i < 38 ? keys[i] : (i == 38 ? '<' : '>'), '\0'};
            draw_text(x + 10, y + 4, 2, key,
                      i == selected ? rgb(2, 30, 42) : rgb(195, 215, 228));
        }
    } else if (strcmp(stage, "HOME") == 0 || strcmp(stage, "DESKTOP") == 0) {
        int desktop = strcmp(stage, "DESKTOP") == 0;
        if (desktop) {
            rectangle(margin + 36, panel_y + 210, w - 2 * margin - 72, 83,
                      rgb(10, 25, 43));
            draw_text(margin + 50, panel_y + 215, 2,
                      "DESKTOP WORKSPACE PREVIEW", rgb(96, 222, 217));
        }
        static const char *cards[5] = {"GAMES", "SETTINGS", "ADD", "LOCK", "POWER"};
        int left = margin + 36;
        int cell_width = (w - 2 * margin - 72) / 5;
        for (int i = 0; i < 5; ++i) {
            int x = left + i * cell_width;
            rectangle(x + 2, panel_y + (desktop ? 243 : 222), cell_width - 6,
                      desktop ? 42 : 64,
                      i == selected ? rgb(45, 193, 169) : rgb(12, 28, 46));
            draw_text(x + 12, panel_y + (desktop ? 256 : 244), 2, cards[i],
                      i == selected ? rgb(2, 30, 42) : rgb(195, 215, 228));
        }
    }
    draw_text(margin + 36, panel_y + h / 2 - 50, 2, message, rgb(89, 230, 195));
    draw_text(margin, h - 55, 2,
              strcmp(stage, "LOCK SCREEN") == 0 ?
              "TAB OR BUMPER SWITCH  DPAD TYPE  START UNLOCK" :
              virtual_keyboard ? "DPAD MOVE  A TYPE  B DELETE  START OK" :
              strcmp(stage, "HOME") == 0 || strcmp(stage, "DESKTOP") == 0 ?
              "DPAD MOVE  A SELECT" :
                                           "KEYBOARD ENTER TO CONTINUE",
              rgb(172, 194, 212));
}
