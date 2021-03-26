#include <linux/module.h>
#include <linux/miscdevice.h> // for misc-driver calls.
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <stdbool.h>
// #error Are we building this?

#define MY_DEVICE_FILE "morse-code"
#define DOT 200
#define DASH 3*DOT
#define SPACE 7*DOT

static unsigned short morsecode_codes[] = {
	0xB800, // A 1011 1
	0xEA80, // B 1110 1010 1
	0xEBA0, // C 1110 1011 101
	0xEA00, // D 1110 101
	0x8000, // E 1
	0xAE80, // F 1010 1110 1
	0xEE80, // G 1110 1110 1
	0xAA00, // H 1010 101
	0xA000, // I 101
	0xBBB8, // J 1011 1011 1011 1
	0xEB80, // K 1110 1011 1
	0xBA80, // L 1011 1010 1
	0xEE00, // M 1110 111
	0xE800, // N 1110 1
	0xEEE0, // O 1110 1110 111
	0xBBA0, // P 1011 1011 101
	0xEEB8, // Q 1110 1110 1011 1
	0xBA00, // R 1011 101
	0xA800, // S 1010 1
	0xE000, // T 111
	0xAE00, // U 1010 111
	0xAB80, // V 1010 1011 1
	0xBB80, // W 1011 1011 1
	0xEAE0, // X 1110 1010 111
	0xEBB8, // Y 1110 1011 1011 1
	0xEEA0	// Z 1110 1110 101
};

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

static void flash_dot(void)
{
	led_trigger_event(morse_code_trigger, LED_FULL);
	msleep(DOT);
	led_trigger_event(morse_code_trigger, LED_OFF);
	msleep(DOT);
}

static void flash_dash(void)
{
	led_trigger_event(morse_code_trigger, LED_FULL);
	msleep(DASH);
	led_trigger_event(morse_code_trigger, LED_OFF);
	msleep(DOT);
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
	if (c >= 'a' && c <= 'z') {
		return c-32;
	}
	return c;
}

static void output_space(void)
{
	led_trigger_event(morse_code_trigger, LED_OFF);
	msleep(SPACE);
}

static void output_letter(char c)
{
	// TODO: prevent extra dot time at the end of a word
	// TODO: implement break after a character
	unsigned short letter_code;
	int bit_index;
	int num_ones;

	num_ones = 0;
	letter_code = morsecode_codes[c-'A'];
	for (bit_index = 15; bit_index >= 0; bit_index--) {
		if (letter_code & (1 << bit_index)) {
			num_ones++;
		} else {
			if (num_ones == 1) {
				flash_dot();
			} else if (num_ones == 3) {
				flash_dash();
			}
			num_ones = 0;
		}
	}

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