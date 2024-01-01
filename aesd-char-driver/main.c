/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/sem.h>
#include "aesdchar.h"
int aesd_major = 0; // use dynamic major
int aesd_minor = 0;

MODULE_AUTHOR("Aleksandr Vinogradov");
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    struct aesd_dev *dev;
    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = dev;
    filp->f_pos = 0;

    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                  loff_t *f_pos)
{
    ssize_t retval = 0;
    PDEBUG("read %zu bytes with offset %lld", count, *f_pos);
    struct aesd_dev *dev = filp->private_data;

    // lock mutex
    if (mutex_lock_interruptible(&dev->buffer_mutex))
    {
        PDEBUG("Error locking mutex");
        return -ERESTARTSYS;
    }
    size_t entry_offset_byte_rtn;
    struct aesd_buffer_entry *entry;
    entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->buffer, *f_pos, &entry_offset_byte_rtn);
    if (entry == NULL)
    {
        PDEBUG("entry is NULL. No data to read");
        goto out;
    }
    PDEBUG("Entry found");
    size_t bytes_to_copy = entry->size - entry_offset_byte_rtn;
    if (bytes_to_copy > count)
    {
        bytes_to_copy = count;
    }
    PDEBUG("Bytes to copy: %zu", bytes_to_copy);

    if (copy_to_user(buf, entry->buffptr + entry_offset_byte_rtn, bytes_to_copy))
    {
        PDEBUG("Error copying to user");
        retval = -EFAULT;
        goto out;
    }

    *f_pos += bytes_to_copy;
    retval = bytes_to_copy;

out:
    // unlock mutex
    mutex_unlock(&dev->buffer_mutex);
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                   loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld", count, *f_pos);
    struct aesd_dev *dev = filp->private_data;

    // lock mutex
    if (mutex_lock_interruptible(&dev->buffer_mutex))
    {
        PDEBUG("Error locking mutex");
        return -ERESTARTSYS;
    }

    // bad algorithm which allocate space each time
    // copy old buffer to new one and free old one
    if (count <= 0)
    {
        PDEBUG("NO WRITE. Count is 0 or less");
        goto out;
    }

    size_t offset = 0;
    // allocate new buffer with size of old buffer + new data
    char *new_buffer = kmalloc(dev->write_buffer_size + count, GFP_KERNEL);
    if (new_buffer == NULL)
    {
        PDEBUG("Error allocating new buffer");
        retval = -ENOMEM;
        goto out;
    }
    if (dev->write_buffer != NULL)
    {
        PDEBUG("Old buffer is not NULL. Copying old buffer to new one");
        // copy old buffer to new one
        if (memcpy(new_buffer, dev->write_buffer, dev->write_buffer_size) == NULL)
        {
            PDEBUG("Error copying old buffer to new one");
            kfree(new_buffer);
            retval = -EFAULT;
            goto out;
        }
        offset = dev->write_buffer_size;
        kfree(dev->write_buffer);
    }
    dev->write_buffer = new_buffer;
    dev->write_buffer_size += count;

    // copy data from user to buffer with offset
    if (copy_from_user(dev->write_buffer + offset, buf, count))
    {
        PDEBUG("Error copying from user");
        retval = -EFAULT;
        goto out;
    }

    retval = count;

    // look for new line in buffer
    char *new_line = memchr(dev->write_buffer, '\n', dev->write_buffer_size);
    // if new line character found, add command to circular buffer until new line
    if (new_line != NULL)
    {
        PDEBUG("New line found");
        struct aesd_buffer_entry entry;
        entry.buffptr = dev->write_buffer;
        entry.size = dev->write_buffer_size;
        const char *old_buffer = aesd_circular_buffer_add_entry(&dev->buffer, &entry);
        if (old_buffer != NULL)
        {
            PDEBUG("Freeing old command");
            kfree(old_buffer);
        }
        dev->write_buffer = NULL;
        dev->write_buffer_size = 0;
    }

out:
    // unlock mutex
    mutex_unlock(&dev->buffer_mutex);
    return retval;
}


loff_t aesd_llseek(struct file * filp, loff_t f_pos, int seek)
{
    if(seek != SEEK_SET)
    {
        PDEBUG("seek other the SEEK_SET is unsupported");
        return -ESPIPE;
    }
    PDEBUG("llseek with f_pos:%lld\n", f_pos);
    PDEBUG("llseek seek is %d\n", seek);
    loff_t retval = 0;
    struct aesd_dev *dev = filp->private_data;

    // lock mutex
    if(mutex_lock_interruptible(&dev->buffer_mutex))
    {
        PDEBUG("Error locking mutex");
        return -ERESTARTSYS;
    }

    if(f_pos < 0)
    {
        PDEBUG("f_pos is less than 0");
        retval = -EINVAL;
        goto out;
    }
    
    filp->f_pos = f_pos;
    retval = filp->f_pos;

out:
    // unlock mutex
    mutex_unlock(&aesd_device.buffer_mutex);
    return retval;
}

struct file_operations aesd_fops = {
    .owner = THIS_MODULE,
    .read = aesd_read,
    .write = aesd_write,
    .open = aesd_open,
    .release = aesd_release,
    .llseek = aesd_llseek,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add(&dev->cdev, devno, 1);
    if (err)
    {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}

int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
                                 "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0)
    {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device, 0, sizeof(struct aesd_dev));

    aesd_device.write_buffer = NULL;
    aesd_device.write_buffer_size = 0;
    aesd_circular_buffer_init(&(aesd_device.buffer));
    mutex_init(&(aesd_device.buffer_mutex));
    PDEBUG("aesd charder inited");

    result = aesd_setup_cdev(&aesd_device);

    if (result)
    {
        unregister_chrdev_region(dev, 1);
    }
    return result;
}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    // free write_buffer if not null
    if (aesd_device.write_buffer != NULL)
    {
        kfree(aesd_device.write_buffer);
    }
    // free circular buffer by adding all entries to free list
    const char *old_buffptr = NULL;
    struct aesd_buffer_entry entry;
    entry.buffptr = NULL;
    entry.size = 0;
    mutex_destroy(&(aesd_device.buffer_mutex));
    do
    {
        old_buffptr = aesd_circular_buffer_add_entry(&(aesd_device.buffer), &entry);
        if (old_buffptr == NULL)
            break;
        kfree(old_buffptr);
    } while (1);

    PDEBUG("aesd chardev cleaned");


    unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
