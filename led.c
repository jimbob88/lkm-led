/*
 * led.c - Trivial led on/off using an LKM.
 */
#include <linux/atomic.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>

#include "led.h"

static int major;

static atomic_t device_status = ATOMIC_INIT(DEVICE_FREE);

static atomic_t counter = ATOMIC_INIT(0);

static char msg[BUF_LEN + 1];

static struct class *cls;

static struct file_operations counter_fops = {
    .owner = THIS_MODULE,
    .read = on_read,
    .write = on_write,
    .open = on_open,
    .release = on_release,
};

static int __init counter_init(void) {
  pr_info("In the beginning...\n");

  major = register_chrdev(0, DEVICE_NAME, &counter_fops);

  if (major < 0) {
    pr_alert("Registering device failed with %d\n", major);
    return major;
  }

  pr_info("Counter assigned major number %d.\n", major);

  cls = class_create(DEVICE_NAME);
  device_create(cls, NULL, MKDEV(major, 0), NULL, DEVICE_NAME);

  pr_info("Device created on /dev/%s\n", DEVICE_NAME);

  return 0;
}

static void __exit counter_exit(void) {
  pr_info("Amen.\n");

  device_destroy(cls, MKDEV(major, 0));
  class_destroy(cls);
  unregister_chrdev(major, DEVICE_NAME);
}

static ssize_t on_read(struct file *file, char __user *buffer, size_t length, loff_t *offset) {
  size_t length_of_msg = strlen(msg);

  if (*offset >= length_of_msg) {
    return 0;
  }

  if (copy_to_user(buffer, msg, length_of_msg)) {
    // Failed to copy the entire message to the buffer.
    return -EFAULT;
  }

  *offset += length_of_msg;

  return length_of_msg;
}

static ssize_t on_write(struct file *file, const char __user *buffer, size_t length, loff_t *offset) {
  pr_alert("Not implemented on write!\n");
  return -1;
}

static int on_open(struct inode *inode, struct file *file) {
  if (atomic_cmpxchg(&device_status, DEVICE_FREE, DEVICE_BOUND)) {
    pr_debug("Device already bound, busy, failed to open!\n");
    return -EBUSY;
  }

  atomic_inc(&counter);
  sprintf(msg, "I have been read %d times.\n", atomic_read(&counter));
  pr_info("%s", msg);

  return 0;
}

static int on_release(struct inode *inode, struct file *file) {
  atomic_set(&device_status, DEVICE_FREE);
  return 0;
}

module_init(counter_init);
module_exit(counter_exit);

MODULE_LICENSE("GPL");
