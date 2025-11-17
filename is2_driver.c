/*
 * i2s_driver.c - I2S Character Device Driver
 * 
 * This driver creates /dev/i2s0 device node for I2S communication
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/slab.h>
#include <linux/mutex.h>

#define DEVICE_NAME "i2s"
#define CLASS_NAME "i2s_class"

/* IOCTL commands */
#define I2S_IOC_MAGIC 'i'
#define I2S_SET_SAMPLE_RATE _IOW(I2S_IOC_MAGIC, 1, int)
#define I2S_GET_SAMPLE_RATE _IOR(I2S_IOC_MAGIC, 2, int)
#define I2S_SET_BIT_DEPTH _IOW(I2S_IOC_MAGIC, 3, int)
#define I2S_GET_BIT_DEPTH _IOR(I2S_IOC_MAGIC, 4, int)
#define I2S_START _IO(I2S_IOC_MAGIC, 5)
#define I2S_STOP _IO(I2S_IOC_MAGIC, 6)
#define I2S_GET_STATUS _IOR(I2S_IOC_MAGIC, 7, int)

/* I2S device state */
struct i2s_dev {
    dev_t dev_num;
    struct cdev cdev;
    struct class *class;
    struct device *device;
    struct mutex lock;
    
    /* I2S configuration */
    int sample_rate;
    int bit_depth;
    int is_running;
    
    /* Buffer for audio data */
    char *buffer;
    size_t buffer_size;
};

static struct i2s_dev *i2s_device;

/* File operations */
static int i2s_open(struct inode *inode, struct file *filp)
{
    filp->private_data = i2s_device;
    pr_info("I2S: Device opened\n");
    return 0;
}

static int i2s_release(struct inode *inode, struct file *filp)
{
    pr_info("I2S: Device closed\n");
    return 0;
}

static ssize_t i2s_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    struct i2s_dev *dev = filp->private_data;
    ssize_t ret = 0;
    
    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;
    
    if (!dev->is_running) {
        pr_warn("I2S: Device not running\n");
        ret = -EINVAL;
        goto out;
    }
    
    /* Simulate reading audio data */
    if (count > dev->buffer_size)
        count = dev->buffer_size;
    
    if (copy_to_user(buf, dev->buffer, count)) {
        ret = -EFAULT;
        goto out;
    }
    
    ret = count;
    pr_debug("I2S: Read %zu bytes\n", count);
    
out:
    mutex_unlock(&dev->lock);
    return ret;
}

static ssize_t i2s_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    struct i2s_dev *dev = filp->private_data;
    ssize_t ret = 0;
    
    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;
    
    if (!dev->is_running) {
        pr_warn("I2S: Device not running\n");
        ret = -EINVAL;
        goto out;
    }
    
    /* Ensure buffer is large enough */
    if (count > dev->buffer_size) {
        kfree(dev->buffer);
        dev->buffer = kmalloc(count, GFP_KERNEL);
        if (!dev->buffer) {
            ret = -ENOMEM;
            goto out;
        }
        dev->buffer_size = count;
    }
    
    if (copy_from_user(dev->buffer, buf, count)) {
        ret = -EFAULT;
        goto out;
    }
    
    /* Here you would write to actual I2S hardware */
    ret = count;
    pr_debug("I2S: Wrote %zu bytes\n", count);
    
out:
    mutex_unlock(&dev->lock);
    return ret;
}

static long i2s_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct i2s_dev *dev = filp->private_data;
    int ret = 0;
    int value;
    
    if (_IOC_TYPE(cmd) != I2S_IOC_MAGIC)
        return -ENOTTY;
    
    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;
    
    switch (cmd) {
    case I2S_SET_SAMPLE_RATE:
        if (copy_from_user(&value, (int __user *)arg, sizeof(int))) {
            ret = -EFAULT;
            break;
        }
        dev->sample_rate = value;
        pr_info("I2S: Sample rate set to %d Hz\n", value);
        break;
        
    case I2S_GET_SAMPLE_RATE:
        if (copy_to_user((int __user *)arg, &dev->sample_rate, sizeof(int)))
            ret = -EFAULT;
        break;
        
    case I2S_SET_BIT_DEPTH:
        if (copy_from_user(&value, (int __user *)arg, sizeof(int))) {
            ret = -EFAULT;
            break;
        }
        dev->bit_depth = value;
        pr_info("I2S: Bit depth set to %d bits\n", value);
        break;
        
    case I2S_GET_BIT_DEPTH:
        if (copy_to_user((int __user *)arg, &dev->bit_depth, sizeof(int)))
            ret = -EFAULT;
        break;
        
    case I2S_START:
        dev->is_running = 1;
        pr_info("I2S: Started\n");
        break;
        
    case I2S_STOP:
        dev->is_running = 0;
        pr_info("I2S: Stopped\n");
        break;
        
    case I2S_GET_STATUS:
        if (copy_to_user((int __user *)arg, &dev->is_running, sizeof(int)))
            ret = -EFAULT;
        break;
        
    default:
        ret = -ENOTTY;
    }
    
    mutex_unlock(&dev->lock);
    return ret;
}

static struct file_operations i2s_fops = {
    .owner = THIS_MODULE,
    .open = i2s_open,
    .release = i2s_release,
    .read = i2s_read,
    .write = i2s_write,
    .unlocked_ioctl = i2s_ioctl,
};

static int __init i2s_driver_init(void)
{
    int ret;
    
    /* Allocate device structure */
    i2s_device = kzalloc(sizeof(struct i2s_dev), GFP_KERNEL);
    if (!i2s_device)
        return -ENOMEM;
    
    /* Allocate device number */
    ret = alloc_chrdev_region(&i2s_device->dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        pr_err("I2S: Failed to allocate device number\n");
        goto err_alloc;
    }
    
    /* Initialize cdev */
    cdev_init(&i2s_device->cdev, &i2s_fops);
    i2s_device->cdev.owner = THIS_MODULE;
    
    ret = cdev_add(&i2s_device->cdev, i2s_device->dev_num, 1);
    if (ret < 0) {
        pr_err("I2S: Failed to add cdev\n");
        goto err_cdev;
    }
    
    /* Create device class */
    i2s_device->class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(i2s_device->class)) {
        ret = PTR_ERR(i2s_device->class);
        pr_err("I2S: Failed to create class\n");
        goto err_class;
    }
    
    /* Create device */
    i2s_device->device = device_create(i2s_device->class, NULL, 
                                       i2s_device->dev_num, NULL, 
                                       DEVICE_NAME"0");
    if (IS_ERR(i2s_device->device)) {
        ret = PTR_ERR(i2s_device->device);
        pr_err("I2S: Failed to create device\n");
        goto err_device;
    }
    
    /* Initialize mutex and default values */
    mutex_init(&i2s_device->lock);
    i2s_device->sample_rate = 44100;
    i2s_device->bit_depth = 16;
    i2s_device->is_running = 0;
    i2s_device->buffer_size = 4096;
    i2s_device->buffer = kmalloc(i2s_device->buffer_size, GFP_KERNEL);
    
    if (!i2s_device->buffer) {
        ret = -ENOMEM;
        goto err_buffer;
    }
    
    pr_info("I2S: Driver loaded successfully\n");
    return 0;
    
err_buffer:
    device_destroy(i2s_device->class, i2s_device->dev_num);
err_device:
    class_destroy(i2s_device->class);
err_class:
    cdev_del(&i2s_device->cdev);
err_cdev:
    unregister_chrdev_region(i2s_device->dev_num, 1);
err_alloc:
    kfree(i2s_device);
    return ret;
}

static void __exit i2s_driver_exit(void)
{
    kfree(i2s_device->buffer);
    device_destroy(i2s_device->class, i2s_device->dev_num);
    class_destroy(i2s_device->class);
    cdev_del(&i2s_device->cdev);
    unregister_chrdev_region(i2s_device->dev_num, 1);
    kfree(i2s_device);
    
    pr_info("I2S: Driver unloaded\n");
}

module_init(i2s_driver_init);
module_exit(i2s_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("I2S Character Device Driver");
MODULE_VERSION("1.0");
