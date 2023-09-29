/**
 * File:	lkmasg1.c
 * Written for Linux 5.19 by: Justin Lugo, Mathew Reilly, Tiuna Benito Saenz
 * Adapted from code written by: John Aedo
 * Class:	COP4600-SP23
 */

#include <linux/module.h>	  // Core header for modules.
#include <linux/device.h>	  // Supports driver model.
#include <linux/kernel.h>	  // Kernel header for convenient functions.
#include <linux/fs.h>		  // File-system support.
#include <linux/uaccess.h>	  // User access copy function support.
#include <linux/mutex.h>
#define DEVICE_NAME "pa2_in" // Device name.
#define CLASS_NAME "charI"	  ///< The device class -- this is a character device driver
#define BUFFER_SIZE 1024	  // Size of buffer; document says 1KB -> 1024 bytes

MODULE_LICENSE("GPL");						 ///< The license type -- this affects available functionality
MODULE_AUTHOR("Justin Lugo, Mathew Reilly, Tiuna Benito Saenz");					 ///< The author -- visible when you use modinfo
MODULE_DESCRIPTION("pa2_in input Kernel Module"); ///< The description -- see modinfo
MODULE_VERSION("0.1");						 ///< A version number to inform users

/**
 * Important variables that store data and keep track of relevant information.
 */

static int major_number;
static int num_opens = 0;
static int error_count = 0;
size_t message_length = 0;
char message[BUFFER_SIZE] = {0};
DEFINE_MUTEX(mutexlock);

EXPORT_SYMBOL(message_length);
EXPORT_SYMBOL(message);
EXPORT_SYMBOL(mutexlock);

static struct class *pa2Class = NULL;	///< The device-driver class struct pointer
static struct device *pa2Device = NULL; ///< The device-driver device struct pointer

/**
 * Prototype functions for file operations.
 */
static int open(struct inode *, struct file *);
static int close(struct inode *, struct file *);
//static ssize_t read(struct file *, char *, size_t, loff_t *);
static ssize_t write(struct file *, const char *, size_t, loff_t *);

/**
 * File operations structure and the functions it points to.
 */
static struct file_operations fops =
	{
		.owner = THIS_MODULE,
		.open = open,
		.release = close,
		//.read = read,
		.write = write,
};

/**
 * Initializes module at installation
 */
int init_module(void)
{
	printk(KERN_INFO "input: installing module.\n");

	// Allocate a major number for the device.
	major_number = register_chrdev(0, DEVICE_NAME, &fops);
	if (major_number < 0)
	{
		printk(KERN_ALERT "input could not register number.\n");
		return major_number;
	}
	printk(KERN_INFO "input: registered correctly with major number %d\n", major_number);

	// Register the device class
	pa2Class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(pa2Class))
	{ // Check for error and clean up if there is
		unregister_chrdev(major_number, DEVICE_NAME);
		printk(KERN_ALERT "Failed to register device class\n");
		return PTR_ERR(pa2Class); // Correct way to return an error on a pointer
	}
	printk(KERN_INFO "input: device class registered correctly\n");

	// Register the device driver
	pa2Device = device_create(pa2Class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
	if (IS_ERR(pa2Device))
	{								 // Clean up if there is an error
		class_destroy(pa2Class); // Repeated code but the alternative is goto statements
		unregister_chrdev(major_number, DEVICE_NAME);
		printk(KERN_ALERT "Failed to create the device\n");
		return PTR_ERR(pa2Device);
	}

	printk(KERN_INFO "input: device class created correctly\n"); // Made it! device was initialized
	
	// We clear the buffer.	
	memset(message, '\0', BUFFER_SIZE);
	mutex_init(&mutexlock);
	printk(KERN_ALERT "input: Awaiting lock.\n");
	return 0;
}

/*
 * Removes module, sends appropriate message to kernel
 */
void cleanup_module(void)
{
	printk(KERN_INFO "input: removing module.\n");
	device_destroy(pa2Class, MKDEV(major_number, 0)); // remove the device
	class_unregister(pa2Class);						  // unregister the device class
	class_destroy(pa2Class);						  // remove the device class
	mutex_destroy(&mutexlock);
	unregister_chrdev(major_number, DEVICE_NAME);		  // unregister the major number
	printk(KERN_INFO "input: Goodbye from the LKM!\n");
	unregister_chrdev(major_number, DEVICE_NAME);
	return;
}

/*
 * Opens device module, sends appropriate message to kernel
 */
static int open(struct inode *inodep, struct file *filep)
{
	num_opens++;
	printk(KERN_INFO "input: Device opened %d times successfully.\n", num_opens);
	return 0;
}

/*
 * Closes device module, sends appropriate message to kernel
 */
static int close(struct inode *inodep, struct file *filep)
{
	printk(KERN_INFO "input: Device closed successfully.\n");
	return 0;
}

/*
 * Writes to the device
 */
static ssize_t write(struct file *filep, const char *buffer, size_t len, loff_t *offset)
{	
	size_t bytes_to_write = len;
	if (bytes_to_write > BUFFER_SIZE - message_length)
	{
		printk(KERN_INFO "input: Inputted string adds up to larger than buffer allots, dropping the rest to fit buffer.\n");
		bytes_to_write = BUFFER_SIZE - message_length;
	}
	
	if (bytes_to_write <= 0)
	{
		printk(KERN_INFO "input: No string written.\n");
		return 0;
	}
	
	if (mutex_lock_interruptible(&mutexlock))
	{
		printk(KERN_INFO "input: Failed to acquire lock.\n");
		return -EBUSY;
	}
	printk(KERN_ALERT "input: Lock acquired.\n");
	printk(KERN_ALERT "input: In Critical Section.\n");

	error_count = copy_from_user(message + message_length, buffer, bytes_to_write);
	message_length += bytes_to_write;
	
	if (error_count == 0)
	{
		printk(KERN_INFO "input: '%s' written to device.\n", message);
		printk(KERN_INFO "input: (%zu characters).\n", message_length);
	}
	else
	{
		printk(KERN_INFO "input: Failed to send %d characters to the user.\n", error_count);
	}

	mutex_unlock(&mutexlock);
	printk(KERN_ALERT "input: Lock released.\n");

	return bytes_to_write;
}
