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

static struct {
    int32_t x;
    int32_t y;
    bool pressed;
} touch_state;

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

static void touch_event_callback(struct input_event *evt, void *user_data)
{
    ARG_UNUSED(user_data);

    if (evt->code == INPUT_ABS_X) {
        touch_state.x = evt->value;
    } else if (evt->code == INPUT_ABS_Y) {
        touch_state.y = evt->value;
    } else if (evt->code == INPUT_BTN_TOUCH) {
        const bool pressed = (evt->value != 0);

        touch_state.pressed = pressed;
        printk("Touch %s: x=%d y=%d\n",
               pressed ? "pressed" : "released",
               (int)touch_state.x, (int)touch_state.y);
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
    if (x >= 480 || y >= 320) {
        return;
    }
    if (x + w > 480) {
        w = 480 - x;
    }
    if (y + h > 320) {
        h = 320 - y;
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
    const uint16_t draw_w = (new_x + DINO_WIDTH <= 480)
                            ? DINO_WIDTH
                            : (uint16_t)(480 - new_x);

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
