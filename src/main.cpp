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
    app_display_fill_screen(display, APP_DISPLAY_BG_COLOR);
    app_display_draw_text_box(display, 8, 8);

    while (true) {
        k_sleep(K_FOREVER);
    }
    return 0;
}
