#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/sys/printk.h>

#include "display/display.h"

#define LCD_BACKLIGHT_NODE DT_ALIAS(pwm_lcd_backlight)
static const pwm_dt_spec backlight = PWM_DT_SPEC_GET(LCD_BACKLIGHT_NODE);

int main()
{
    if (pwm_is_ready_dt(&backlight)) {
        pwm_set_pulse_dt(&backlight, backlight.period);
    }

    const struct device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (!device_is_ready(display)) {
        printk("Display not ready\n");
        return 1;
    }

    if (!app_display_touch_ready()) {
        printk("Touch not ready\n");
    }

    display_blanking_off(display);
    app_display_fill_screen(display, 0xFFFFu);

    while (app_display_mount_animation_storage() != 0) {
        k_sleep(K_SECONDS(1));
    }

    while (app_display_load_dino(display) != 0) {
        printk("Waiting for /SD:/dino.rgb565 ...\n");
        k_sleep(K_SECONDS(1));
    }

    uint8_t step = 0;

    while (true) {
        app_display_step_dino(display, step);
        step = (step + 1U) % APP_DISPLAY_DINO_STEPS;
        k_sleep(APP_DISPLAY_ANIMATION_DELAY);
    }

    while (true) {
        k_sleep(K_FOREVER);
    }
    return 0;
}
