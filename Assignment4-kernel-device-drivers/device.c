#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/semaphore.h>
#include <linux/device.h>

MODULE_LICENSE("GPL");

#define DEVICE_NAME "dd"

static int tasks;
module_param( tasks, int, S_IRUGO );

static DEFINE_SEMAPHORE( full );
static DEFINE_SEMAPHORE( empty );
static DEFINE_MUTEX( mutex );

static char **line;
int copyRet, checkOut, arrIndex1 = 0, msgLengthWrite, arrIndex2 = 0, msgLengthRead;

// device driver for producer to write to buffer
static ssize_t dev_write( struct file *file, const char __user * out, size_t size, loff_t * off ) {
	if( down_interruptible( &empty ) < 0 ) return -1;
	if( mutex_lock_interruptible( &mutex ) < 0 ) return -1;
	if( arrIndex1 == tasks ) arrIndex1 = 0;

	checkOut = access_ok( VERIFY_READ, out, sizeof( *out ) );
	if ( !checkOut ) return -1;

	copyRet = copy_from_user( line[arrIndex1], out, size );
	msgLengthWrite = strlen( line[arrIndex1] );
	arrIndex1++;
	mutex_unlock( &mutex );
	up( &full );
	return msgLengthWrite;
}

// device driver for consumer to read from buffer
static ssize_t dev_read( struct file *file, char __user * out, size_t size, loff_t * off ) {
	if( down_interruptible( &full ) < 0 ) return -1;
	if( mutex_lock_interruptible( &mutex ) < 0 ) return 0;
	if( arrIndex2 == tasks ) arrIndex2 = 0;
	checkOut = access_ok( VERIFY_WRITE, out, sizeof( *out ) );
	if( !checkOut ) return -1;

	copyRet = copy_to_user( out, line[arrIndex2], size );
	if( copyRet > 0 ) return -1;

	msgLengthRead = strlen( line[arrIndex2] );
	arrIndex2++;
	mutex_unlock( &mutex );
	up( &empty );

	return msgLengthRead;
}

static int dev_open( struct inode *inode, struct file *file ) {
	printk( KERN_ALERT "Device open for operation\n" );
	return 0;
}

static struct file_operations fops = {
	.write = dev_write,
	.read = dev_read,
	.open = dev_open
};

static struct miscdevice misc_device = {
	.name = "dd",
	.fops = &fops,
	.minor = MISC_DYNAMIC_MINOR
};

int __init init_module( void ) {
	int i, reg_ret;
	// initializing semaphores and mutex
	sema_init( &empty, tasks );
	sema_init( &full, 0 );
	mutex_init( &mutex );
	reg_ret = misc_register( &misc_device ); // device driver register

	if( reg_ret < 0 ) {
		printk( KERN_ALERT "Device driver cannot be created!\n" );
		return reg_ret;
	}
	line = kmalloc(sizeof(char *) * (tasks), GFP_KERNEL);

	for(i = 0; i < tasks; i++)
		line[i] = kmalloc(sizeof(char *) * (100), GFP_KERNEL);

	printk( KERN_ALERT "Kernel module start\n" );
	return 0;
}

void __exit cleanup_module( void ) {
	printk( KERN_ALERT "Kernel module end\n" );
	kfree( line ); // freeing the allocated memory
	misc_deregister( &misc_device ); // deregistring the driver
}