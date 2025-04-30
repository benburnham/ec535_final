#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/timer.h>
#include <linux/seq_file.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>

/*---------------------------------------------------
    Prepared by Ben Burnham and Mateen Sharif
    EC523 Intro to Embedded Systems
    Final Project
---------------------------------------------------*/

MODULE_LICENSE("Dual BSD/GPL");
#define DEVICE_NAME "myco2"
#define SERVO_PIN 68    // Signal to Arduino: HIGH => open servo, LOW => close servo
#define CO2_THRESH_PIN 44   // Signal from Arduino: HIGH => above thresh, LOW => below
#define BTN0_PIN 26     // Mode switch button
#define BTN1_PIN 46     // Manual mode button

// 3 operating modes
#define CO2_MODE    0
#define TIMER_MODE  1
#define MANUAL_MODE 2

// Sub‐state for CO2 logic
#define CO2_STATE_WAITING   0
#define CO2_STATE_OPENING   1
#define CO2_STATE_COOLDOWN  2

// Init vars
static int myco2_major  = 61;
static int current_mode = CO2_MODE;
static int co2_state    = CO2_STATE_WAITING;
static int co2_counter  = 0;
static int cycle_rate   = 1;    // 1 Hz by default
static int cycle_count  = 0;
static int timer_open_time  = 5;    // default: open 5 cycles
static int timer_close_time = 10;   // default: close 10 cycles
static int co2_open_time    = 10;   // default: open 10 cycles
static int co2_hold_time    = 5;    // default: open 5 cycles

// Kernel timer object
static struct timer_list traffic_timer;

static void update_vent_control(struct timer_list *t);
static ssize_t myco2_read(struct file *file, char __user *buf, size_t len, loff_t *offset);
static ssize_t myco2_write(struct file *file, const char __user *buf, size_t len, loff_t *offset);
static irqreturn_t btn0_irq_handler(int irq, void *dev_id);
static irqreturn_t btn1_irq_handler(int irq, void *dev_id);

static const struct file_operations myco2_fops = {
    .read  = myco2_read,
    .write = myco2_write,
};


static int __init myco2_init(void) {
    int result;

    // Register character device
    result = register_chrdev(myco2_major, DEVICE_NAME, &myco2_fops);
    if (result < 0) {
        pr_alert("myco2: cannot obtain major number %d\n", myco2_major);
        return result;
    }

    // Request and configure GPIO pins:
    gpio_request(SERVO_PIN, "SERVO_PIN");
    gpio_direction_output(SERVO_PIN, 0);  // start servo “closed”

    gpio_direction_input(CO2_THRESH_PIN);
    gpio_request(CO2_THRESH_PIN, "CO2_THRESH_PIN");

    gpio_request(BTN0_PIN, "BTN0_PIN");
    gpio_direction_input(BTN0_PIN);
    gpio_set_debounce(BTN0_PIN, 200);

    gpio_request(BTN1_PIN, "BTN1_PIN");
    gpio_direction_input(BTN1_PIN);
    gpio_set_debounce(BTN1_PIN, 200);

    // Setup IRQs on the two buttons
    if (request_irq(gpio_to_irq(BTN0_PIN), btn0_irq_handler, IRQF_TRIGGER_RISING, "BTN0_IRQ", NULL)) {
        pr_alert("Failed to register IRQ for BTN0\n");
        goto fail;
    }
    if (request_irq(gpio_to_irq(BTN1_PIN), btn1_irq_handler, IRQF_TRIGGER_RISING, "BTN1_IRQ", NULL)) {
        pr_alert("Failed to register IRQ for BTN1\n");
        free_irq(gpio_to_irq(BTN0_PIN), NULL);
        goto fail;
    }

    // Initialize kernel timer
    timer_setup(&traffic_timer, update_vent_control, 0);
    mod_timer(&traffic_timer, jiffies + HZ / cycle_rate);

    pr_info("CO2 Vent module loaded successfully\n");
    return 0;

    // In case of failure during init
    fail:
        gpio_free(SERVO_PIN);
        gpio_free(CO2_THRESH_PIN);
        gpio_free(BTN0_PIN);
        gpio_free(BTN1_PIN);
        unregister_chrdev(myco2_major, DEVICE_NAME);
        return -1;
}


static void __exit myco2_exit(void) {
    free_irq(gpio_to_irq(BTN0_PIN), NULL);
    free_irq(gpio_to_irq(BTN1_PIN), NULL);

    gpio_free(SERVO_PIN);
    gpio_free(CO2_THRESH_PIN);
    gpio_free(BTN0_PIN);
    gpio_free(BTN1_PIN);

    del_timer_sync(&traffic_timer);

    unregister_chrdev(myco2_major, DEVICE_NAME);
    pr_info("CO2 Vent (GPIO-based) module unloaded\n");
}

module_init(myco2_init);
module_exit(myco2_exit);


// Timer Callback, state handler
static void update_vent_control(struct timer_list *t) {
    switch (current_mode) {
        case CO2_MODE:
        {
            // Read CO2 pin from Arduino
            // HIGH => open open servo
            // LOW  => close servo after 5 cycles
            int co2_high = gpio_get_value(CO2_THRESH_PIN);

            switch (co2_state) {
                case CO2_STATE_WAITING:
                    // If CO2 pin is high => open => go to OPENING state
                    if (co2_high) {
                        gpio_set_value(SERVO_PIN, 1);
                        co2_state   = CO2_STATE_OPENING;
                        co2_counter = 0;
                    } else {
                        gpio_set_value(SERVO_PIN, 0);
                    }
                    break;

                case CO2_STATE_OPENING:
                    // Remain open for co2_open_time cycles
                    if (co2_counter >= co2_open_time) {
                        gpio_set_value(SERVO_PIN, 0);
                        co2_state   = CO2_STATE_COOLDOWN;
                        co2_counter = 0;
                    } else {
                        co2_counter++;
                    }
                    break;

                case CO2_STATE_COOLDOWN:
                    // We ignore CO2 for 5 cycles
                    if (co2_counter >= co2_hold_time) {
                        co2_state = CO2_STATE_WAITING;
                    } else {
                        co2_counter++;
                    }
                    break;
                }
            break;
        }

        case TIMER_MODE:
        {
            // Servo open for X cycles, then closed for Y cycles, then repeat
            if (cycle_count <= timer_open_time) {
                gpio_set_value(SERVO_PIN, 1);
            } else if (cycle_count < (timer_open_time + timer_close_time)) {
                gpio_set_value(SERVO_PIN, 0);
            } else {
                cycle_count = 0;
            }
            cycle_count++;
            break;
        }

        case MANUAL_MODE:
        {
            // Keep servo open
            gpio_set_value(SERVO_PIN, 1);
            break;
        }

        default:
            break;
        }

    // Re-arm the timer
    mod_timer(&traffic_timer, jiffies + HZ / cycle_rate);
}


static ssize_t myco2_read(struct file *file, char __user *buf, size_t len, loff_t *offset) {
    char status[256];
    const char *mode_str;
    int n;

    switch (current_mode) {
    case CO2_MODE:    mode_str = "CO2";    break;
    case TIMER_MODE:  mode_str = "Timer";  break;
    case MANUAL_MODE: mode_str = "Manual"; break;
    default:          mode_str = "Unknown";break;
    }

    n = snprintf(status, sizeof(status),
        "Mode: %s\n"
        "manual_mode: %s\n"
        "timer_open_time: %d cycles\n"
        "timer_close_time: %d cycles\n"
        "co2_open_time: %d cycles\n"
        "co2_substate: %d (0=WAIT,1=OPEN,2=COOLDOWN)\n"
        "SERVO_PIN=%d, CO2_THRESH_PIN=%d\n\n",
        mode_str,
        (current_mode == MANUAL_MODE) ? "true" : "false",
        timer_open_time,
        timer_close_time,
        co2_open_time,
        co2_state,
        gpio_get_value(SERVO_PIN),
        gpio_get_value(CO2_THRESH_PIN)
    );

    if (*offset >= n)
        return 0;
    if (len > n - *offset)
        len = n - *offset;

    if (copy_to_user(buf, status + *offset, len))
        return -EFAULT;

    *offset += len;
    return len;
}


// Use write to update mode parameters
static ssize_t myco2_write(struct file *file, const char __user *buf, size_t len, loff_t *offset) {
    char input[32];
    int  val;

    if (len >= sizeof(input))
        return -EINVAL;

    if (copy_from_user(input, buf, len))
        return -EFAULT;

    input[len] = '\0';

    // parse the commands:
    //   co2time <val>
    //   timeropen <val>
    //   timerclose <val>
    if (sscanf(input, "co2time %d", &val) == 1) {
        co2_open_time = val;
    } else if (sscanf(input, "timeropen %d", &val) == 1) {
        timer_open_time = val;
    } else if (sscanf(input, "timerclose %d", &val) == 1) {
        timer_close_time = val;
    }

    return len;
}


// BTN0 => toggles between CO2 and TIMER if not in MANUAL
static irqreturn_t btn0_irq_handler(int irq, void *dev_id)
{
    if (current_mode != MANUAL_MODE) {
        if (current_mode == CO2_MODE){
            current_mode = TIMER_MODE;
        } else {
            current_mode = CO2_MODE;
            co2_state    = CO2_STATE_WAITING;
            co2_counter  = 0;
        }
        cycle_count = 0;
    }
    return IRQ_HANDLED;
}


// BTN1 => toggles MANUAL mode. If turning off, revert to CO2
static irqreturn_t btn1_irq_handler(int irq, void *dev_id)
{
    if (current_mode == MANUAL_MODE) {
        current_mode = CO2_MODE;
        co2_state    = CO2_STATE_WAITING;
        co2_counter  = 0;
        cycle_count  = 0;
    } else {
        current_mode = MANUAL_MODE;
    }
    return IRQ_HANDLED;
}
