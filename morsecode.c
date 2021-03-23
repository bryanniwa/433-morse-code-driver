#include <linux/module.h>
#include <linux/miscdevice.h> // for misc-driver calls.
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
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
	int buf_index;

	for (buf_index = 0; buf_index < count; buf_index++) {
		char c;
		if (copy_from_user(&c, &buff[buf_index], sizeof(c))) {
			return -EFAULT;
		}

		if (c < 'A' || (c > 'Z' && c < 'a') || (c > 'z')) {
			continue;
		}

		// TODO: Remove this before final submission
		printk(KERN_INFO "%c\n", c);
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