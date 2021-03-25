#include <linux/module.h>
#include <linux/miscdevice.h> // for misc-driver calls.
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <stdbool.h>
// #error Are we building this?

#define MY_DEVICE_FILE "morse-code"
#define DOT 200
#define DASH DOT*3

/******************************************************
 * LED
 ******************************************************/
#include <linux/leds.h>

DEFINE_LED_TRIGGER(morse_code_trigger);

static void led_register(void)
{
	led_trigger_register_simple("morse-code", &morse_code_trigger);
}

static void led_unregister(void)
{
	led_trigger_unregister_simple(morse_code_trigger);
}

/******************************************************
 * Helpers
 ******************************************************/
static int get_start_index(const char* buff, size_t size)
{
	int start = 0;
	while (start < size) {
		char c;
		if (copy_from_user(&c, &buff[start], sizeof(c))) {
			return -EFAULT;
		}

		if (c == ' ' || c == '\n') {
			start++;
		} else {
			break;
		}
	}
	return start;
}

static int get_end_index(const char* buff, size_t size)
{
	int end = size-1;
	while (end > 0) {
		char c;
		if (copy_from_user(&c, &buff[end], sizeof(c))) {
			return -EFAULT;
		}

		if (c == ' ' || c == '\n') {
			end--;
		} else {
			break;
		}
	}
	return end;
}

static char get_upper(char c)
{
	if (c >= 65 && c <= 90) {
		return c-32;
	}
	return c;
}

static void output_space(void)
{
	printk(KERN_INFO "\n");
}

static void output_letter(char c)
{
	printk(KERN_INFO "%c\n", c);
}
/******************************************************
 * Callbacks
 ******************************************************/
static ssize_t my_read(struct file *file,
					   char *buff, size_t count, loff_t *ppos)
{
	printk(KERN_INFO "morsecode: In my_read()\n");
	return 0; // # bytes actually read.
}

static ssize_t my_write(struct file *file,
						const char *buff, size_t count, loff_t *ppos)
{
	int buff_index;
	int end;
	bool space_waiting;

	buff_index = get_start_index(buff, count);
	end = get_end_index(buff, count);
	space_waiting = false;

	for (buff_index; buff_index <= end; buff_index++) {
		char c;
		if (copy_from_user(&c, &buff[buff_index], sizeof(c))) {
			return -EFAULT;
		}

		if ((c < 'A' || (c > 'Z' && c < 'a') || (c > 'z')) && c != ' ') {
			continue;
		}

		if (c == ' ') {
			space_waiting = true;
			continue;
		}

		if (space_waiting) {
			output_space();
			space_waiting = false;
		}

		output_letter(get_upper(c));
	}
	// Just return count for now to exit nicely
	// TODO: make this return the actual number of bytes written
	return count;
}

/******************************************************
 * Misc support
 ******************************************************/
// Callbacks:  (structure defined in /linux/fs.h)
struct file_operations my_fops = {
	.owner = THIS_MODULE,
	.read = my_read,
	.write = my_write
};

// Character Device info for the Kernel:
static struct miscdevice my_miscdevice = {
	.minor = MISC_DYNAMIC_MINOR, // Let the system assign one.
	.name = MY_DEVICE_FILE,		 // /dev/.... file.
	.fops = &my_fops			 // Callback functions.
};

/******************************************************
 * Driver initialization and exit:
 ******************************************************/
static int __init my_init(void)
{
	int ret;
	printk(KERN_INFO "----> morsecode driver init(): file /dev/%s.\n", MY_DEVICE_FILE);

	// Register as a misc driver:
	ret = misc_register(&my_miscdevice);

	// Register our LED trigger
	led_register();

	return ret;
}

static void __exit my_exit(void)
{
	printk(KERN_INFO "<---- morsecode driver exit().\n");

	// Unregister misc driver
	misc_deregister(&my_miscdevice);

	// Unregister LED trigger
	led_unregister();
}

module_init(my_init);
module_exit(my_exit);

MODULE_AUTHOR("Bryan Niwa & Ali Khamesy");
MODULE_DESCRIPTION("A driver to convert strings into morse code");
MODULE_LICENSE("GPL");