#include "display/display.h"

#include <zephyr/drivers/display.h>
#include <zephyr/fs/fs.h>
#include <zephyr/kernel.h>
#include <zephyr/input/input.h>
#include <zephyr/storage/disk_access.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/printk.h>

#include <ff.h>

static const struct device *const touch = DEVICE_DT_GET(DT_CHOSEN(zephyr_touch));
static constexpr const char *ANIMATION_DISK        = "SD";
static constexpr const char *ANIMATION_MOUNT_POINT = "/SD:";
static constexpr const char *DINO_FILE_PATH        = "/SD:/dino.rgb565";
static constexpr const char *GROUND_FILE_PATH      = "/SD:/ground.rgb565";
static constexpr const char *CACTUS_FILE_PATH      = "/SD:/cactus.rgb565";

static constexpr uint16_t DINO_WIDTH  = 113;
static constexpr uint16_t DINO_HEIGHT = 100;
static constexpr uint16_t DINO_Y      = 30;
static constexpr uint16_t DINO_STEP_X = 21;
static constexpr size_t   DINO_PIXELS = (size_t)DINO_WIDTH * DINO_HEIGHT;
static constexpr size_t   DINO_SIZE   = DINO_PIXELS * sizeof(uint16_t);
static constexpr uint16_t GROUND_WIDTH  = 65;
static constexpr uint16_t GROUND_HEIGHT = 20;
static constexpr size_t   GROUND_PIXELS = (size_t)GROUND_WIDTH * GROUND_HEIGHT;
static constexpr size_t   GROUND_SIZE   = GROUND_PIXELS * sizeof(uint16_t);
static constexpr uint16_t CACTUS_WIDTH  = 97;
static constexpr uint16_t CACTUS_HEIGHT = 95;
static constexpr size_t   CACTUS_PIXELS = (size_t)CACTUS_WIDTH * CACTUS_HEIGHT;
static constexpr size_t   CACTUS_SIZE   = CACTUS_PIXELS * sizeof(uint16_t);
static constexpr uint16_t SCREEN_WIDTH  = 480;
static constexpr uint16_t SCREEN_HEIGHT = 320;
static constexpr uint16_t START_BUTTON_WIDTH = 208;
static constexpr uint16_t START_BUTTON_HEIGHT = 82;
static constexpr uint16_t START_BUTTON_X = (SCREEN_WIDTH - START_BUTTON_WIDTH) / 2;
static constexpr uint16_t START_BUTTON_Y = SCREEN_HEIGHT - START_BUTTON_HEIGHT - 40;
static constexpr uint16_t START_BUTTON_COLOR = 0x8410;
static constexpr uint16_t START_TEXT_COLOR = 0xFFFF;
static constexpr uint16_t START_BORDER_COLOR = 0xBDF7;
static constexpr uint16_t EXIT_BUTTON_WIDTH = 100;
static constexpr uint16_t EXIT_BUTTON_HEIGHT = 60;
static constexpr uint16_t EXIT_BUTTON_X = SCREEN_WIDTH - EXIT_BUTTON_WIDTH - 20;
static constexpr uint16_t EXIT_BUTTON_Y = 20;
static constexpr uint16_t GROUND_Y = SCREEN_HEIGHT - GROUND_HEIGHT - 20;
static constexpr uint16_t GAME_DINO_X = 36;
static constexpr uint16_t GAME_DINO_Y = GROUND_Y - DINO_HEIGHT;
static constexpr uint16_t GAME_REDRAW_Y = GAME_DINO_Y;
static constexpr uint16_t GAME_REDRAW_HEIGHT = SCREEN_HEIGHT - GAME_REDRAW_Y;
static constexpr uint16_t CACTUS_Y = GROUND_Y - CACTUS_HEIGHT;
static constexpr int16_t  JUMP_VELOCITY = -35;
static constexpr int16_t  GRAVITY = 5;
static constexpr int16_t  GAME_TICK_X = 17;
static constexpr uint16_t GAME_OVER_BOX_WIDTH = 280;
static constexpr uint16_t GAME_OVER_BOX_HEIGHT = 96;
static constexpr uint16_t GAME_OVER_BOX_X = (SCREEN_WIDTH - GAME_OVER_BOX_WIDTH) / 2;
static constexpr uint16_t GAME_OVER_BOX_Y = 58;
static constexpr int16_t  CACTUS_GAP_MIN = 250;
static constexpr int16_t  CACTUS_GAP_RANGE = 90;

static struct {
    int32_t x;
    int32_t y;
    bool pressed;
    bool start_pressed;
    bool exit_pressed;
    bool jump_pressed;
    bool restart_pressed;
} touch_state;

static app_display_screen current_screen = APP_DISPLAY_SCREEN_MENU;

static FATFS fat_fs;
static struct fs_mount_t animation_mount = {
    .type     = FS_FATFS,
    .mnt_point = ANIMATION_MOUNT_POINT,
    .fs_data  = &fat_fs,
};
static bool     animation_storage_ready;
static uint16_t dino_buf[DINO_PIXELS];
static uint16_t ground_buf[GROUND_PIXELS];
static uint16_t cactus_buf[CACTUS_PIXELS];
static bool     dino_loaded;
static bool     ground_loaded;
static bool     cactus_loaded;
static uint16_t dino_prev_x = UINT16_MAX; /* sentinel: no previous position */
static int16_t  game_dino_y = GAME_DINO_Y;
static int16_t  game_dino_velocity;
static int16_t  cactus_x[2] = { (int16_t)(SCREEN_WIDTH + 40), (int16_t)(SCREEN_WIDTH + 280) };
static bool     game_over;
static uint32_t cactus_rng_state = 0x13572468U;

static constexpr uint8_t FONT_S[7] = {
    0x1E, 0x10, 0x10, 0x1E, 0x02, 0x02, 0x1E,
};
static constexpr uint8_t FONT_T[7] = {
    0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
};
static constexpr uint8_t FONT_A[7] = {
    0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11,
};
static constexpr uint8_t FONT_R[7] = {
    0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11,
};
static constexpr uint8_t FONT_E[7] = {
    0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F,
};
static constexpr uint8_t FONT_X[7] = {
    0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11,
};
static constexpr uint8_t FONT_I[7] = {
    0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F,
};
static constexpr uint8_t FONT_G[7] = {
    0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E,
};
static constexpr uint8_t FONT_M[7] = {
    0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11,
};
static constexpr uint8_t FONT_O[7] = {
    0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E,
};
static constexpr uint8_t FONT_V[7] = {
    0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04,
};

static void update_touch_buttons(void)
{
    const bool in_start =
        touch_state.x >= (int32_t)START_BUTTON_X &&
        touch_state.x <  (int32_t)(START_BUTTON_X + START_BUTTON_WIDTH) &&
        touch_state.y >= (int32_t)START_BUTTON_Y &&
        touch_state.y <  (int32_t)(START_BUTTON_Y + START_BUTTON_HEIGHT);
    const bool in_exit =
        touch_state.x >= (int32_t)EXIT_BUTTON_X &&
        touch_state.x <  (int32_t)(EXIT_BUTTON_X + EXIT_BUTTON_WIDTH) &&
        touch_state.y >= (int32_t)EXIT_BUTTON_Y &&
        touch_state.y <  (int32_t)(EXIT_BUTTON_Y + EXIT_BUTTON_HEIGHT);
    const bool in_restart =
        touch_state.x >= (int32_t)GAME_OVER_BOX_X &&
        touch_state.x <  (int32_t)(GAME_OVER_BOX_X + GAME_OVER_BOX_WIDTH) &&
        touch_state.y >= (int32_t)GAME_OVER_BOX_Y &&
        touch_state.y <  (int32_t)(GAME_OVER_BOX_Y + GAME_OVER_BOX_HEIGHT);

    if (current_screen == APP_DISPLAY_SCREEN_MENU && in_start) {
        touch_state.start_pressed = true;
    } else if (current_screen == APP_DISPLAY_SCREEN_GAME && in_exit) {
        touch_state.exit_pressed = true;
    } else if (current_screen == APP_DISPLAY_SCREEN_GAME && game_over && in_restart) {
        touch_state.restart_pressed = true;
    } else if (current_screen == APP_DISPLAY_SCREEN_GAME && !game_over) {
        touch_state.jump_pressed = true;
    }
}

static void touch_event_callback(struct input_event *evt, void *user_data)
{
    ARG_UNUSED(user_data);

    if (evt->code == INPUT_ABS_X) {
        touch_state.x = (int32_t)SCREEN_WIDTH - 1 - evt->value;
    } else if (evt->code == INPUT_ABS_Y) {
        touch_state.y = evt->value;
    } else if (evt->code == INPUT_BTN_TOUCH) {
        const bool was_pressed = touch_state.pressed;
        touch_state.pressed = (evt->value != 0);

        /* Fire on release so X/Y are fully settled from this touch. */
        if (was_pressed && !touch_state.pressed) {
            update_touch_buttons();
        }
    } else {
        return;
    }
}

INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(DT_CHOSEN(zephyr_touch)), touch_event_callback, NULL);

/* Static row buffer used by fill_rect and fill_screen */
static uint16_t fill_row[480];

static void normalize_buf_for_display(const struct device *dev, uint16_t *buf, size_t count)
{
    struct display_capabilities caps;

    display_get_capabilities(dev, &caps);

    if (caps.current_pixel_format != PIXEL_FORMAT_RGB_565X) {
        return;
    }

    for (size_t i = 0; i < count; i++) {
        buf[i] = sys_cpu_to_be16(buf[i]);
    }
}

static int read_exact_file(const char *path, void *buf, size_t size)
{
    struct fs_file_t file;
    ssize_t rc;
    size_t total = 0;

    fs_file_t_init(&file);

    rc = fs_open(&file, path, FS_O_READ);
    if (rc < 0) {
        printk("Open failed for %s (%d)\n", path, (int)rc);
        return (int)rc;
    }

    while (total < size) {
        rc = fs_read(&file, (uint8_t *)buf + total, size - total);
        if (rc < 0) {
            printk("Read failed for %s (%d)\n", path, (int)rc);
            fs_close(&file);
            return (int)rc;
        }
        if (rc == 0) {
            printk("Short read for %s (%u/%u)\n",
                   path, (unsigned int)total, (unsigned int)size);
            fs_close(&file);
            return -EIO;
        }

        total += (size_t)rc;
    }

    fs_close(&file);
    return 0;
}

bool app_display_touch_ready(void)
{
    return device_is_ready(touch);
}

int app_display_mount_animation_storage(void)
{
    int rc;

    if (animation_storage_ready) {
        return 0;
    }

    rc = disk_access_init(ANIMATION_DISK);
    if (rc != 0) {
        printk("SD init failed (%d)\n", rc);
        return rc;
    }

    rc = fs_mount(&animation_mount);
    if (rc < 0) {
        printk("SD mount failed (%d)\n", rc);
        return rc;
    }

    animation_storage_ready = true;
    return 0;
}

int app_display_load_dino(const struct device *dev)
{
    if (!animation_storage_ready) {
        return -ENODEV;
    }

    int rc = read_exact_file(DINO_FILE_PATH, dino_buf, DINO_SIZE);
    if (rc < 0) {
        return rc;
    }

    normalize_buf_for_display(dev, dino_buf, DINO_PIXELS);
    dino_loaded = true;
    dino_prev_x = UINT16_MAX;
    return 0;
}

int app_display_load_game_assets(const struct device *dev)
{
    int rc = app_display_load_dino(dev);
    if (rc < 0) {
        return rc;
    }

    rc = read_exact_file(GROUND_FILE_PATH, ground_buf, GROUND_SIZE);
    if (rc < 0) {
        return rc;
    }

    normalize_buf_for_display(dev, ground_buf, GROUND_PIXELS);
    ground_loaded = true;

    rc = read_exact_file(CACTUS_FILE_PATH, cactus_buf, CACTUS_SIZE);
    if (rc < 0) {
        return rc;
    }

    normalize_buf_for_display(dev, cactus_buf, CACTUS_PIXELS);
    cactus_loaded = true;
    return 0;
}

/* Fill a rectangle with a solid color, one row at a time. */
static void fill_rect(const struct device *dev,
                      uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                      uint16_t color)
{
    if (x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT) {
        return;
    }
    if (x + w > SCREEN_WIDTH) {
        w = SCREEN_WIDTH - x;
    }
    if (y + h > SCREEN_HEIGHT) {
        h = SCREEN_HEIGHT - y;
    }

    for (uint16_t i = 0; i < w; i++) {
        fill_row[i] = color;
    }

    struct display_buffer_descriptor desc = {
        .buf_size = (uint32_t)w * sizeof(uint16_t),
        .width    = w,
        .height   = 1,
        .pitch    = w,
    };

    for (uint16_t row = y; row < y + h; row++) {
        display_write(dev, x, row, &desc, fill_row);
    }
}

static void draw_letter(const struct device *dev, uint16_t x, uint16_t y,
                        const uint8_t *glyph, uint16_t scale, uint16_t color)
{
    for (uint16_t row = 0; row < 7; row++) {
        for (uint16_t col = 0; col < 5; col++) {
            if ((glyph[row] & (1U << (4U - col))) == 0U) {
                continue;
            }

            fill_rect(dev, x + col * scale, y + row * scale, scale, scale, color);
        }
    }
}

static void draw_rgb565_image(const struct device *dev, int16_t x, int16_t y,
                              uint16_t width, uint16_t height,
                              const uint16_t *buf)
{
    if (x >= (int16_t)SCREEN_WIDTH || y >= (int16_t)SCREEN_HEIGHT) {
        return;
    }
    if (x + (int16_t)width <= 0 || y + (int16_t)height <= 0) {
        return;
    }

    uint16_t draw_w = width;
    uint16_t draw_h = height;
    uint16_t src_x = 0;
    uint16_t src_y = 0;

    if (x < 0) {
        src_x = (uint16_t)(-x);
        draw_w -= src_x;
        x = 0;
    }
    if (y < 0) {
        src_y = (uint16_t)(-y);
        draw_h -= src_y;
        y = 0;
    }

    if (x + (int16_t)draw_w > (int16_t)SCREEN_WIDTH) {
        draw_w = (uint16_t)(SCREEN_WIDTH - x);
    }
    if (y + (int16_t)draw_h > (int16_t)SCREEN_HEIGHT) {
        draw_h = (uint16_t)(SCREEN_HEIGHT - y);
    }

    struct display_buffer_descriptor desc = {
        .buf_size = (uint32_t)draw_w * sizeof(uint16_t),
        .width = draw_w,
        .height = 1,
        .pitch = width,
    };

    for (uint16_t row = 0; row < draw_h; row++) {
        display_write(dev, (uint16_t)x, (uint16_t)(y + row), &desc,
                      &buf[(row + src_y) * width + src_x]);
    }
}

static void draw_gameplay_layer(const struct device *dev)
{
    fill_rect(dev, 0, GAME_REDRAW_Y, SCREEN_WIDTH, GAME_REDRAW_HEIGHT, APP_DISPLAY_BG_COLOR);

    if (ground_loaded) {
        for (int16_t x = 0; x < (int16_t)SCREEN_WIDTH; x += GROUND_WIDTH) {
            draw_rgb565_image(dev, x, GROUND_Y, GROUND_WIDTH, GROUND_HEIGHT, ground_buf);
        }
    }

    if (cactus_loaded) {
        for (size_t i = 0; i < ARRAY_SIZE(cactus_x); i++) {
            if (cactus_x[i] + CACTUS_WIDTH > 0 && cactus_x[i] < SCREEN_WIDTH) {
                draw_rgb565_image(dev, cactus_x[i], CACTUS_Y,
                                  CACTUS_WIDTH, CACTUS_HEIGHT, cactus_buf);
            }
        }
    }

    if (dino_loaded) {
        draw_rgb565_image(dev, GAME_DINO_X, game_dino_y, DINO_WIDTH, DINO_HEIGHT, dino_buf);
    }
}

static void restore_ground_tiles(const struct device *dev, int16_t x, int16_t width)
{
    if (!ground_loaded) {
        return;
    }

    int16_t start = MAX(0, x);
    int16_t end = MIN((int16_t)SCREEN_WIDTH, (int16_t)(x + width));

    if (start >= end) {
        return;
    }

    int16_t tile_x = (start / (int16_t)GROUND_WIDTH) * (int16_t)GROUND_WIDTH;
    for (; tile_x < end; tile_x += GROUND_WIDTH) {
        draw_rgb565_image(dev, tile_x, GROUND_Y, GROUND_WIDTH, GROUND_HEIGHT, ground_buf);
    }
}

static void clear_game_rect(const struct device *dev, int16_t x, int16_t y,
                            int16_t width, int16_t height)
{
    int16_t x0 = MAX(0, x);
    int16_t y0 = MAX(0, y);
    int16_t x1 = MIN((int16_t)SCREEN_WIDTH, (int16_t)(x + width));
    int16_t y1 = MIN((int16_t)SCREEN_HEIGHT, (int16_t)(y + height));

    if (x0 >= x1 || y0 >= y1) {
        return;
    }

    fill_rect(dev, (uint16_t)x0, (uint16_t)y0, (uint16_t)(x1 - x0), (uint16_t)(y1 - y0),
              APP_DISPLAY_BG_COLOR);

    if (y1 > (int16_t)GROUND_Y && y0 < (int16_t)(GROUND_Y + GROUND_HEIGHT)) {
        restore_ground_tiles(dev, x0, x1 - x0);
    }
}

static uint32_t next_cactus_random(void)
{
    cactus_rng_state = cactus_rng_state * 1664525U + 1013904223U;
    return cactus_rng_state;
}

static int16_t next_cactus_gap(void)
{
    return (int16_t)(CACTUS_GAP_MIN + (next_cactus_random() % CACTUS_GAP_RANGE));
}

static void draw_game_over_overlay(const struct device *dev)
{
    static constexpr uint16_t border = 3;
    static constexpr uint16_t letter_scale = 5;
    static constexpr uint16_t letter_width = 5 * letter_scale;
    static constexpr uint16_t letter_height = 7 * letter_scale;
    static constexpr uint16_t letter_gap = 6;
    static constexpr uint16_t text_width = 8 * letter_width + 7 * letter_gap;
    static constexpr uint16_t text_x = GAME_OVER_BOX_X + (GAME_OVER_BOX_WIDTH - text_width) / 2;
    static constexpr uint16_t text_y = GAME_OVER_BOX_Y + 14;

    fill_rect(dev, GAME_OVER_BOX_X, GAME_OVER_BOX_Y,
              GAME_OVER_BOX_WIDTH, GAME_OVER_BOX_HEIGHT, START_BORDER_COLOR);
    fill_rect(dev, GAME_OVER_BOX_X + border, GAME_OVER_BOX_Y + border,
              GAME_OVER_BOX_WIDTH - (2 * border),
              GAME_OVER_BOX_HEIGHT - (2 * border), START_BUTTON_COLOR);

    draw_letter(dev, text_x, text_y, FONT_G, letter_scale, START_TEXT_COLOR);
    draw_letter(dev, text_x + 1 * (letter_width + letter_gap), text_y,
                FONT_A, letter_scale, START_TEXT_COLOR);
    draw_letter(dev, text_x + 2 * (letter_width + letter_gap), text_y,
                FONT_M, letter_scale, START_TEXT_COLOR);
    draw_letter(dev, text_x + 3 * (letter_width + letter_gap), text_y,
                FONT_E, letter_scale, START_TEXT_COLOR);
    draw_letter(dev, text_x + 4 * (letter_width + letter_gap), text_y,
                FONT_O, letter_scale, START_TEXT_COLOR);
    draw_letter(dev, text_x + 5 * (letter_width + letter_gap), text_y,
                FONT_V, letter_scale, START_TEXT_COLOR);
    draw_letter(dev, text_x + 6 * (letter_width + letter_gap), text_y,
                FONT_E, letter_scale, START_TEXT_COLOR);
    draw_letter(dev, text_x + 7 * (letter_width + letter_gap), text_y,
                FONT_R, letter_scale, START_TEXT_COLOR);

    fill_rect(dev, GAME_OVER_BOX_X + 26, GAME_OVER_BOX_Y + GAME_OVER_BOX_HEIGHT - 24,
              GAME_OVER_BOX_WIDTH - 52, 4, START_TEXT_COLOR);
}

void app_display_draw_start_button(const struct device *dev)
{
    static constexpr uint16_t border = 4;
    static constexpr uint16_t letter_scale = 6;
    static constexpr uint16_t letter_width = 5 * letter_scale;
    static constexpr uint16_t letter_height = 7 * letter_scale;
    static constexpr uint16_t letter_gap = 8;
    static constexpr uint16_t text_width = 5 * letter_width + 4 * letter_gap;
    static constexpr uint16_t text_x = START_BUTTON_X + (START_BUTTON_WIDTH - text_width) / 2;
    static constexpr uint16_t text_y = START_BUTTON_Y + (START_BUTTON_HEIGHT - letter_height) / 2;

    fill_rect(dev, START_BUTTON_X, START_BUTTON_Y,
              START_BUTTON_WIDTH, START_BUTTON_HEIGHT, START_BORDER_COLOR);
    fill_rect(dev, START_BUTTON_X + border, START_BUTTON_Y + border,
              START_BUTTON_WIDTH - (2 * border),
              START_BUTTON_HEIGHT - (2 * border), START_BUTTON_COLOR);

    draw_letter(dev, text_x, text_y, FONT_S, letter_scale, START_TEXT_COLOR);
    draw_letter(dev, text_x + (letter_width + letter_gap), text_y,
                FONT_T, letter_scale, START_TEXT_COLOR);
    draw_letter(dev, text_x + 2 * (letter_width + letter_gap), text_y,
                FONT_A, letter_scale, START_TEXT_COLOR);
    draw_letter(dev, text_x + 3 * (letter_width + letter_gap), text_y,
                FONT_R, letter_scale, START_TEXT_COLOR);
    draw_letter(dev, text_x + 4 * (letter_width + letter_gap), text_y,
                FONT_T, letter_scale, START_TEXT_COLOR);
}

void app_display_draw_exit_button(const struct device *dev)
{
    static constexpr uint16_t border = 3;
    static constexpr uint16_t letter_scale = 4;
    static constexpr uint16_t letter_width = 5 * letter_scale;
    static constexpr uint16_t letter_height = 7 * letter_scale;
    static constexpr uint16_t letter_gap = 4;
    static constexpr uint16_t text_width = 4 * letter_width + 3 * letter_gap;
    static constexpr uint16_t text_x = EXIT_BUTTON_X + (EXIT_BUTTON_WIDTH - text_width) / 2;
    static constexpr uint16_t text_y = EXIT_BUTTON_Y + (EXIT_BUTTON_HEIGHT - letter_height) / 2;

    fill_rect(dev, EXIT_BUTTON_X, EXIT_BUTTON_Y,
              EXIT_BUTTON_WIDTH, EXIT_BUTTON_HEIGHT, START_BORDER_COLOR);
    fill_rect(dev, EXIT_BUTTON_X + border, EXIT_BUTTON_Y + border,
              EXIT_BUTTON_WIDTH - (2 * border),
              EXIT_BUTTON_HEIGHT - (2 * border), START_BUTTON_COLOR);

    draw_letter(dev, text_x, text_y, FONT_E, letter_scale, START_TEXT_COLOR);
    draw_letter(dev, text_x + (letter_width + letter_gap), text_y,
                FONT_X, letter_scale, START_TEXT_COLOR);
    draw_letter(dev, text_x + 2 * (letter_width + letter_gap), text_y,
                FONT_I, letter_scale, START_TEXT_COLOR);
    draw_letter(dev, text_x + 3 * (letter_width + letter_gap), text_y,
                FONT_T, letter_scale, START_TEXT_COLOR);
}

void app_display_draw_game_screen(const struct device *dev)
{
    app_display_fill_screen(dev, APP_DISPLAY_BG_COLOR);
    app_display_draw_exit_button(dev);
    draw_gameplay_layer(dev);
    if (game_over) {
        draw_game_over_overlay(dev);
    }
}

bool app_display_take_start_pressed(void)
{
    const bool pressed = touch_state.start_pressed;

    touch_state.start_pressed = false;
    if (pressed) {
        current_screen = APP_DISPLAY_SCREEN_GAME;
    }

    return pressed;
}

bool app_display_take_exit_pressed(void)
{
    const bool pressed = touch_state.exit_pressed;

    touch_state.exit_pressed = false;
    if (pressed) {
        current_screen = APP_DISPLAY_SCREEN_MENU;
    }

    return pressed;
}

bool app_display_take_jump_pressed(void)
{
    const bool pressed = touch_state.jump_pressed;

    touch_state.jump_pressed = false;
    return pressed;
}

static bool app_display_take_restart_pressed(void)
{
    const bool pressed = touch_state.restart_pressed;

    touch_state.restart_pressed = false;
    return pressed;
}

void app_display_reset_game(void)
{
    game_dino_y = GAME_DINO_Y;
    game_dino_velocity = 0;
    cactus_x[0] = (int16_t)(SCREEN_WIDTH + 40);
    cactus_x[1] = (int16_t)(cactus_x[0] + next_cactus_gap());
    touch_state.jump_pressed = false;
    touch_state.restart_pressed = false;
    game_over = false;
}

static bool dino_collides_with_cactus(int16_t cactus_left)
{
    const int16_t dino_left = GAME_DINO_X + 18;
    const int16_t dino_right = GAME_DINO_X + DINO_WIDTH - 18;
    const int16_t dino_top = game_dino_y + 14;
    const int16_t dino_bottom = game_dino_y + DINO_HEIGHT - 10;
    const int16_t cactus_left_hit = cactus_left + 4 + 4;
    const int16_t cactus_right = cactus_left + CACTUS_WIDTH - 4 - 16;
    const int16_t cactus_top = CACTUS_Y + 4 + 12;
    const int16_t cactus_bottom = CACTUS_Y + CACTUS_HEIGHT - 4 - 4;

    return dino_left < cactus_right &&
           dino_right > cactus_left_hit &&
           dino_top < cactus_bottom &&
           dino_bottom > cactus_top;
}

void app_display_step_game(const struct device *dev)
{
    if (!dino_loaded || !ground_loaded || !cactus_loaded) {
        return;
    }

    if (game_over) {
        if (app_display_take_restart_pressed()) {
            app_display_reset_game();
            app_display_draw_game_screen(dev);
        }
        return;
    }

    if (app_display_take_jump_pressed() && game_dino_y == GAME_DINO_Y) {
        game_dino_velocity = JUMP_VELOCITY;
    }

    const int16_t old_dino_y = game_dino_y;
    const int16_t old_cactus_x0 = cactus_x[0];
    const int16_t old_cactus_x1 = cactus_x[1];

    if (game_dino_y < GAME_DINO_Y || game_dino_velocity < 0) {
        game_dino_y += game_dino_velocity;
        game_dino_velocity += GRAVITY;
        if (game_dino_y > GAME_DINO_Y) {
            game_dino_y = GAME_DINO_Y;
            game_dino_velocity = 0;
        }
    }

    for (size_t i = 0; i < ARRAY_SIZE(cactus_x); i++) {
        cactus_x[i] -= GAME_TICK_X;
    }

    if (cactus_x[0] + CACTUS_WIDTH < 0) {
        cactus_x[0] = (int16_t)(cactus_x[1] + next_cactus_gap());
    }
    if (cactus_x[1] + CACTUS_WIDTH < 0) {
        cactus_x[1] = (int16_t)(cactus_x[0] + next_cactus_gap());
    }

    if (dino_collides_with_cactus(cactus_x[0]) ||
        dino_collides_with_cactus(cactus_x[1])) {
        game_over = true;
        app_display_draw_game_screen(dev);
        return;
    }

    if (old_dino_y != game_dino_y) {
        if (game_dino_y < old_dino_y) {
            clear_game_rect(dev, GAME_DINO_X,
                            old_dino_y + DINO_HEIGHT - (old_dino_y - game_dino_y),
                            DINO_WIDTH,
                            old_dino_y - game_dino_y);
        } else {
            clear_game_rect(dev, GAME_DINO_X,
                            old_dino_y,
                            DINO_WIDTH,
                            game_dino_y - old_dino_y);
        }
    }

    if (cactus_x[0] > old_cactus_x0) {
        clear_game_rect(dev, old_cactus_x0, CACTUS_Y, CACTUS_WIDTH, CACTUS_HEIGHT);
    } else {
        clear_game_rect(dev, old_cactus_x0 + CACTUS_WIDTH - GAME_TICK_X, CACTUS_Y,
                        GAME_TICK_X, CACTUS_HEIGHT);
    }

    if (cactus_x[1] > old_cactus_x1) {
        clear_game_rect(dev, old_cactus_x1, CACTUS_Y, CACTUS_WIDTH, CACTUS_HEIGHT);
    } else {
        clear_game_rect(dev, old_cactus_x1 + CACTUS_WIDTH - GAME_TICK_X, CACTUS_Y,
                        GAME_TICK_X, CACTUS_HEIGHT);
    }

    for (size_t i = 0; i < ARRAY_SIZE(cactus_x); i++) {
        if (cactus_x[i] + CACTUS_WIDTH > 0 && cactus_x[i] < SCREEN_WIDTH) {
            draw_rgb565_image(dev, cactus_x[i], CACTUS_Y,
                              CACTUS_WIDTH, CACTUS_HEIGHT, cactus_buf);
        }
    }
    if (old_dino_y != game_dino_y) {
        draw_rgb565_image(dev, GAME_DINO_X, game_dino_y, DINO_WIDTH, DINO_HEIGHT, dino_buf);
    }
}

void app_display_reset_dino_animation(void)
{
    dino_prev_x = UINT16_MAX;
}

void app_display_step_dino(const struct device *dev, uint8_t step)
{
    if (!dino_loaded) {
        return;
    }

    const uint16_t new_x = (uint16_t)step * DINO_STEP_X;

    /* Clear the strip that the dino is leaving behind. */
    if (dino_prev_x != UINT16_MAX) {
        if (new_x > dino_prev_x) {
            /* Moving right: clear the left strip exposed by the move. */
            const uint16_t strip_w = new_x - dino_prev_x;
            fill_rect(dev, dino_prev_x, DINO_Y, strip_w, DINO_HEIGHT,
                      APP_DISPLAY_BG_COLOR);
        } else {
            /* Wrapped back to start: clear the entire old position. */
            fill_rect(dev, dino_prev_x, DINO_Y, DINO_WIDTH, DINO_HEIGHT,
                      APP_DISPLAY_BG_COLOR);
        }
    }

    /* Draw the dino at the new position, clipped to screen width. */
    const uint16_t draw_w = (new_x + DINO_WIDTH <= SCREEN_WIDTH)
                            ? DINO_WIDTH
                            : (uint16_t)(SCREEN_WIDTH - new_x);

    struct display_buffer_descriptor desc = {
        .buf_size = (uint32_t)draw_w * sizeof(uint16_t),
        .width    = draw_w,
        .height   = 1,
        .pitch    = DINO_WIDTH,
    };

    for (uint16_t row = 0; row < DINO_HEIGHT; row++) {
        display_write(dev, new_x, DINO_Y + row, &desc,
                      &dino_buf[row * DINO_WIDTH]);
    }

    dino_prev_x = new_x;
}

void app_display_fill_screen(const struct device *dev, uint16_t color)
{
    struct display_capabilities caps;
    display_get_capabilities(dev, &caps);

    uint16_t w = MIN(caps.x_resolution, (uint16_t)ARRAY_SIZE(fill_row));
    for (uint16_t i = 0; i < w; i++) {
        fill_row[i] = color;
    }

    struct display_buffer_descriptor desc = {
        .buf_size = w * sizeof(uint16_t),
        .width = w,
        .height = 1,
        .pitch = w,
    };

    for (uint16_t y = 0; y < caps.y_resolution; y++) {
        display_write(dev, 0, y, &desc, fill_row);
    }
}
