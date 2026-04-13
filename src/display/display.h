#ifndef APP_DISPLAY_H_
#define APP_DISPLAY_H_

#include <stdint.h>
#include <zephyr/device.h>

static constexpr uint16_t APP_DISPLAY_BG_COLOR = 0xF800u;

bool app_display_touch_ready(void);
void app_display_fill_screen(const struct device *dev, uint16_t color);
void app_display_draw_text_box(const struct device *dev, uint16_t x, uint16_t y);

#endif /* APP_DISPLAY_H_ */
