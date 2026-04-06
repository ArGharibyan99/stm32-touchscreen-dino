#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

int main()
{
    printk("Hello from STM32 Nucleo-F429ZI on Zephyr!\n");

    while (true) {
        printk("Application main() is running\n");
        k_sleep(K_SECONDS(1));
    }

    return 0;
}
