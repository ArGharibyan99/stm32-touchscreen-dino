#ifndef APP_DISPLAY_H_
#define APP_DISPLAY_H_

#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>

static constexpr uint16_t APP_DISPLAY_BG_COLOR  = 0xFFFFu; /* white screen background */
static constexpr uint8_t  APP_DISPLAY_DINO_STEPS = 20;
static constexpr k_timeout_t APP_DISPLAY_ANIMATION_DELAY = K_MSEC(100);

bool app_display_touch_ready(void);
int  app_display_mount_animation_storage(void);
int  app_display_load_dino(const struct device *dev);
void app_display_draw_start_button(const struct device *dev);
void app_display_step_dino(const struct device *dev, uint8_t step);
void app_display_fill_screen(const struct device *dev, uint16_t color);

#endif /* APP_DISPLAY_H_ */
