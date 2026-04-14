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

static constexpr uint16_t DINO_WIDTH  = 113;
static constexpr uint16_t DINO_HEIGHT = 100;
static constexpr uint16_t DINO_Y      = 30;
static constexpr uint16_t DINO_STEP_X = 21;
static constexpr size_t   DINO_PIXELS = (size_t)DINO_WIDTH * DINO_HEIGHT;
static constexpr size_t   DINO_SIZE   = DINO_PIXELS * sizeof(uint16_t);
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

static struct {
    int32_t x;
    int32_t y;
    bool pressed;
    bool start_pressed;
    bool exit_pressed;
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
static bool     dino_loaded;
static uint16_t dino_prev_x = UINT16_MAX; /* sentinel: no previous position */

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

    if (current_screen == APP_DISPLAY_SCREEN_MENU && in_start) {
        touch_state.start_pressed = true;
    } else if (current_screen == APP_DISPLAY_SCREEN_GAME && in_exit) {
        touch_state.exit_pressed = true;
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
