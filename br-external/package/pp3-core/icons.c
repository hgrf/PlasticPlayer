#include "icons.h"

#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>

#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#define FB_DEV "/dev/fb0"
#define FONT_PATH "/usr/share/fonts/truetype/material-symbols.ttf"

static int g_fb_fd;
static uint8_t *g_display;
static size_t g_screen_size;
static unsigned g_screen_width;
static unsigned g_screen_height;
static FT_Library g_library;
static FT_Face g_face;

static void draw_bitmap(FT_Bitmap* bitmap, FT_Int x, FT_Int y, FT_Int offset_x, FT_Int offset_y, FT_Int width, FT_Int height)
{
    FT_Int i, j, p, q;
    FT_Int x_max = x + width;
    FT_Int y_max = y + height;
    for (i = x, p = 0; i < x_max; i++) {
        for (j = y, q = 0; j < y_max; j++) {
            /* if outside of the screen, continue */
            if (i < 0 || j < 0 || i >= g_screen_width  || j >= g_screen_height )
                continue;

            /* if outside of the bitmap, clear the pixel */
            if (i < x + offset_x || j < y + offset_y ||
                i >= x + offset_x + bitmap->width || j >= y + offset_y + bitmap->rows) {
                g_display[(j * g_screen_width + i) / 8] &= ~(1 << (i % 8));
                continue;
            }

            /* otherwise draw the pixel */
            if (bitmap->buffer[q * bitmap->pitch + p / 8] & 0x80 >> (p % 8)) {
                g_display[(j * g_screen_width + i) / 8] |= 1 << (i % 8);
            } else {
                g_display[(j * g_screen_width + i) / 8] &= ~(1 << (i % 8));
            }
            q++;
        }
        if (i >= x + offset_x)
            p++;
    }
}

int icons_init(void) {
    struct fb_var_screeninfo screen_info;
    FT_Error error;

    g_fb_fd = open(FB_DEV, O_RDWR);
    if (g_fb_fd < 0) {
        perror("failed to open frame buffer.\n");
        return -1;
    }

    if (ioctl(g_fb_fd, FBIOGET_VSCREENINFO, &screen_info) < 0) {
        perror("ioctl failed.\n");
        return -1;
    }

    g_screen_size = screen_info.xres * screen_info.yres * screen_info.bits_per_pixel / 8;
    // NOTE: Instead of using a memory-mapped buffer, we write directly to the framebuffer FB with
    //       icons_sync(), because I have not yet figured out how to force the display to be updated
    //       immediately with the memory-mapped buffer, and the refresh rate of 1 Hz (see ssd1307fb
    //       driver) is too laggy.
    g_display = malloc(g_screen_size);
    // g_display = (uint8_t *) mmap(0, g_screen_size, PROT_READ | PROT_WRITE, MAP_SHARED, g_fb_fd, 0);
    // if (g_display == MAP_FAILED) {
    //     perror("mmap failed.\n");
    //     close(g_fb_fd);
    //     return -1;
    // }

    g_screen_width = screen_info.xres;
    g_screen_height = screen_info.yres;

    error = FT_Init_FreeType(&g_library);
    if (error) {
        fprintf(stderr, "FT_Init_FreeType failed\n");
        munmap(g_display, g_screen_size);
        close(g_fb_fd);
        return -1;
    }

    error = FT_New_Face(g_library, FONT_PATH, 0, &g_face);
    if (error) {
        fprintf(stderr, "FT_New_Face failed: %d\n", error);
        FT_Done_FreeType(g_library);
        munmap(g_display, g_screen_size);
        close(g_fb_fd);
        return -1;
    }

    error = FT_Set_Char_Size(g_face, 0, 20*64, 0, 0);
    if (error) {
        fprintf(stderr, "FT_Set_Char_Size failed: %d\n", error);
        FT_Done_Face(g_face);
        FT_Done_FreeType(g_library);
        munmap(g_display, g_screen_size);
        close(g_fb_fd);
        return -1;
    }

    return 0;
}

void icons_put(int x, int y, unsigned icon) {
    FT_Error error;
    FT_UInt glyph_index;
    
    glyph_index = FT_Get_Char_Index(g_face, icon);

    error = FT_Load_Glyph(g_face, glyph_index, FT_LOAD_MONOCHROME);
    if (error) {
        fprintf(stderr, "FT_Load_Glyph failed: %d\n", error);
        return;
    }

    error = FT_Render_Glyph(g_face->glyph, FT_RENDER_MODE_MONO);
    if (error) {
        fprintf(stderr, "FT_Render_Glyph failed: %d\n", error);
        return;
    }

    draw_bitmap(&g_face->glyph->bitmap,
        x, y,
        g_face->glyph->bitmap_left, g_face->size->metrics.height / 64 - g_face->glyph->bitmap_top,
        g_face->size->metrics.max_advance / 64, g_face->size->metrics.height / 64);
}

void icons_clear(void) {
    memset(g_display, 0, g_screen_size);
}

void icons_sync(void) {
    write(g_fb_fd, g_display, g_screen_size / 2);
    close(g_fb_fd);
    g_fb_fd = open(FB_DEV, O_RDWR);
}

void icons_deinit(void) {
    FT_Done_Face(g_face);
    FT_Done_FreeType(g_library);
    // munmap(g_display, g_screen_size);
    close(g_fb_fd);
}
