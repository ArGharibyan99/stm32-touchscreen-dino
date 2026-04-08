#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define LED0_NODE DT_ALIAS(led0)

#if !DT_NODE_HAS_STATUS(LED0_NODE, okay)
#error "Unsupported board: led0 devicetree alias is not defined"
#endif

static const gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

int main()
{
    int ret;

    if (!gpio_is_ready_dt(&led)) {
        printk("LED GPIO device is not ready\n");
        return 1;
    }

    ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
    if (ret != 0) {
        printk("Failed to configure LED GPIO: %d\n", ret);
        return ret;
    }

    printk("Blinking Nucleo LED from Zephyr main()\n");

    while (true) {
        gpio_pin_toggle_dt(&led);
        k_sleep(K_SECONDS(1));
    }

    return 0;
}
