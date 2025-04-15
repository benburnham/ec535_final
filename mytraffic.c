#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/timer.h>
#include <linux/seq_file.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>

/******************************************************
    Prepared by Ben Burnham and Mateen Sharif
    EC523 Intro to Embedded Systems
    Lab 4: traffic light controller

******************************************************/

MODULE_LICENSE("Dual BSD/GPL");

#define DEVICE_NAME "mytraffic"
#define RED_LED     67
#define YELLOW_LED  68
#define GREEN_LED   44
#define BTN0_PIN    26
#define BTN1_PIN    46

static int mytrafic_major = 61;
static int cycle_rate = 1;      // Default cycle rate (1 Hz)
static int current_mode = 0;    // 0: Normal, 1: Flashing-Red, 2: Flashing-Yellow
static int light_state = 0;
static bool pedestrian_present = false;
static struct timer_list traffic_timer;

static void update_traffic_lights(struct timer_list *t);
static ssize_t mytraffic_read(struct file *file, char __user *buf, size_t len, loff_t *offset);
static ssize_t mytraffic_write(struct file *file, const char __user *buf, size_t len, loff_t *offset);
static irqreturn_t btn0_irq_handler(int irq, void *dev_id);
static irqreturn_t btn1_irq_handler(int irq, void *dev_id);

static const struct file_operations mytrafic_fops = {
    .read = mytraffic_read,
    .write = mytraffic_write,
};

static int mytraffic_init(void)
{
    // Register device
    int result = register_chrdev(mytrafic_major, DEVICE_NAME, &mytrafic_fops);
    if (result < 0) {
        printk(KERN_ALERT "mytrafic: cannot obtain major number %d\n", mytrafic_major);
        return result;
    }

    // Setup GPIO pins
    gpio_request(RED_LED, "RED_LED");
    gpio_request(YELLOW_LED, "YELLOW_LED");
    gpio_request(GREEN_LED, "GREEN_LED");
    gpio_request(BTN0_PIN, "BTN0_PIN");
    gpio_request(BTN1_PIN, "BTN1_PIN");

    // Set direction, these can be error checked
    gpio_direction_output(RED_LED, 0);
    gpio_direction_output(YELLOW_LED, 0);
    gpio_direction_output(GREEN_LED, 0);
    gpio_direction_input(BTN0_PIN);
    gpio_direction_input(BTN1_PIN);

    // Setup IRQs for button presses
    if (request_irq(gpio_to_irq(BTN0_PIN), btn0_irq_handler, IRQF_TRIGGER_RISING, "BTN0_IRQ", NULL)) {
        printk(KERN_ALERT "Failed to register IRQ for BTN0\n");
        return -1;
    }
    if (request_irq(gpio_to_irq(BTN1_PIN), btn1_irq_handler, IRQF_TRIGGER_RISING, "BTN1_IRQ", NULL)) {
        printk(KERN_ALERT "Failed to register IRQ for BTN1\n");
        return -1;
    }

    // Initialize kernel timer
    timer_setup(&traffic_timer, update_traffic_lights, 0);
    mod_timer(&traffic_timer, jiffies + HZ / cycle_rate);

    printk(KERN_INFO "Traffic light module loaded successfully\n");
    return 0;
}

static void mytraffic_exit(void)
{
    // Free all resources
    free_irq(gpio_to_irq(BTN0_PIN), NULL);
    free_irq(gpio_to_irq(BTN1_PIN), NULL);

    gpio_free(RED_LED);
    gpio_free(YELLOW_LED);
    gpio_free(GREEN_LED);
    gpio_free(BTN0_PIN);
    gpio_free(BTN1_PIN);

    del_timer_sync(&traffic_timer);

    unregister_chrdev(mytrafic_major, DEVICE_NAME);
    printk(KERN_INFO "Traffic light module unloaded successfully\n");
}


static void update_traffic_lights(struct timer_list *t)
{
    static bool pedestrian_acitve = false;
    static int pedestrian_cycle_count = 0; // Tracks pedestrian wait time

    switch (current_mode) {
        case 0: // Normal mode
            if (pedestrian_acitve) {
                // Pedestrian red+yellow for 5 cycles
                if (pedestrian_cycle_count < 4) {
                    pedestrian_cycle_count++;
                } else {
                    pedestrian_present = false;
                    pedestrian_acitve = false;
                    pedestrian_cycle_count = 0;
                    light_state = 0;    // Restart normal cycle
                }
            } else {
                if (light_state == 0) { // Green for 3 cycles
                    gpio_set_value(RED_LED, 0);
                    gpio_set_value(YELLOW_LED, 0);
                    gpio_set_value(GREEN_LED, 1);
                } else if (light_state == 3) {  // Yellow for 1 cycle
                    gpio_set_value(GREEN_LED, 0);
                    gpio_set_value(YELLOW_LED, 1);
                } else if (light_state == 4) {  // Red for 2 cycles
                    gpio_set_value(RED_LED, 1);
                    if (pedestrian_present) {   // Check for pedestrian
                        gpio_set_value(YELLOW_LED, 1);
                        pedestrian_acitve = true;
                    } else {
                        gpio_set_value(YELLOW_LED, 0);
                    }
                }

                light_state = (light_state + 1) % 6;
            }
            break;

        case 1: // Flashing-Red mode
            gpio_set_value(RED_LED, light_state % 2);
            gpio_set_value(YELLOW_LED, 0);
            gpio_set_value(GREEN_LED, 0);
            light_state = (light_state + 1) % 2;
            break;

        case 2: // Flashing-Yellow mode
            gpio_set_value(RED_LED, 0);
            gpio_set_value(YELLOW_LED, light_state % 2);
            gpio_set_value(GREEN_LED, 0);
            light_state = (light_state + 1) % 2;
            break;
    }

    // Restart timer
    mod_timer(&traffic_timer, jiffies + HZ / cycle_rate);
}


static ssize_t mytraffic_read(struct file *file, char __user *buf, size_t len, loff_t *offset)
{
    char status[256];
    int status_len = snprintf(status, sizeof(status),
                              "Mode: %s\nCycle rate: %d Hz\n"
                              "Red: %s\nYellow: %s\nGreen: %s\n"
                              "Pedestrian present: %s\n\n",
                              current_mode == 0 ? "normal" : (current_mode == 1 ? "flashing-red" : "flashing-yellow"),
                              cycle_rate,
                              gpio_get_value(RED_LED) ? "on" : "off",
                              gpio_get_value(YELLOW_LED) ? "on" : "off",
                              gpio_get_value(GREEN_LED) ? "on" : "off",
                              pedestrian_present ? "yes" : "no");

    if (*offset >= status_len)
        return 0;

    if (len > status_len - *offset)
        len = status_len - *offset;

    if (copy_to_user(buf, status + *offset, len))
        return -EFAULT;

    *offset += len;
    return len;
}


static ssize_t mytraffic_write(struct file *file, const char __user *buf, size_t len, loff_t *offset)
{
    char input[4];
    int new_rate;

    // Prevent buffer overflow
    if (len > sizeof(input) - 1)
        return -EINVAL;

    // Copy data and null terminate
    if (copy_from_user(input, buf, len))
        return -EFAULT;
    input[len] = '\0';

    // Convert input string to integer and validate range
    if (kstrtoint(strim(input), 10, &new_rate) == 0) {
        if (new_rate >= 1 && new_rate <= 9) {
            cycle_rate = new_rate;
        }
    }

    return len;
}

static irqreturn_t btn0_irq_handler(int irq, void *dev_id)
{
    // Switch between modes on BTN0 press (cycle between 0, 1, and 2)
    current_mode = (current_mode + 1) % 3;
    light_state = 0;
    return IRQ_HANDLED;
}

static irqreturn_t btn1_irq_handler(int irq, void *dev_id)
{
    // Toggle pedestrian presence on BTN1 press (only in Normal mode)
    if (current_mode == 0) {
        pedestrian_present = true;
    }
    return IRQ_HANDLED;
}

module_init(mytraffic_init);
module_exit(mytraffic_exit);
