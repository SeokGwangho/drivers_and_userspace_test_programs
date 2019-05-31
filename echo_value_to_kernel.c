#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define DEV_NAME "shi_dev"

struct shi_test_drv_dev_t
{
    dev_t id;
    struct cdev cdev;
    struct class *class;
    struct device *dev;
} *shi_test_drv_dev;

static int shi_test_drv_open(struct inode *inode, struct file *filp)
{
    pr_emerg("%s\n", __func__);
    return 0;
}

static int shi_test_drv_close(struct inode *inode, struct file * filp)
{
    pr_emerg("%s\n", __func__);
    return 0;
}

static ssize_t shi_test_drv_read(struct file *filp, char __user *buf, size_t size, loff_t *offset)
{
    pr_emerg("%s\n", __func__);
    return 0;
}

//userspace: echo 99 > /dev/shi_dev
static ssize_t shi_test_drv_write(struct file *filp, const char __user *buf, size_t size, loff_t *offset)
{
	char kbuf[10] = {0};
	int ret;

	pr_emerg("%s: user_buf: %p, count: %d, ppos: %lld\n", __func__, buf, size, *offset);
	ret = copy_from_user(kbuf, buf, size);
	if (ret) {
		pr_err("%s: write error\n", __func__);
		return -EIO;
	}

	*offset += size;
	pr_emerg("%s: kbuf: %s\n", __func__, kbuf);	//echo value

	return size;
}

static long shi_test_drv_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    pr_emerg("ioctl cmd %d arg %ld\n", cmd, arg);
    return 0;
}

struct file_operations shi_test_drv_fops = {
    .owner          = THIS_MODULE,
    .open           = shi_test_drv_open,
    .release        = shi_test_drv_close,
    .read           = shi_test_drv_read,
    .write          = shi_test_drv_write,
    .unlocked_ioctl = shi_test_drv_ioctl,
};

static int __init shi_test_drv_init(void)
{
    int err;

    pr_emerg("%s\n", __func__);

    shi_test_drv_dev = kmalloc(sizeof(struct shi_test_drv_dev_t), GFP_KERNEL);
    if (!shi_test_drv_dev) {
        pr_err("kmalloc err\n");
        return -ENOMEM;
    }

    err = alloc_chrdev_region(&shi_test_drv_dev->id, 0, 1, DEV_NAME);
    if (err) {
        pr_err("alloc_chrdev_region err %d\n", err);
        goto out_free;
    }

    cdev_init(&shi_test_drv_dev->cdev, &shi_test_drv_fops);
    shi_test_drv_dev->cdev.owner = THIS_MODULE;

    err = cdev_add(&shi_test_drv_dev->cdev, shi_test_drv_dev->id, 1);
    if (err) {
        pr_err("cdev_add err %d\n", err);
        goto out_unregister;
    }

    shi_test_drv_dev->class = class_create(THIS_MODULE, DEV_NAME);
    if (IS_ERR(shi_test_drv_dev->class)) {
        err = PTR_ERR(shi_test_drv_dev->class);
        pr_err("class_create err %d\n", err);
        goto out_cdev_del;
    }

    shi_test_drv_dev->dev = device_create(shi_test_drv_dev->class, NULL,
            shi_test_drv_dev->id, NULL, DEV_NAME);
    if (IS_ERR(shi_test_drv_dev->dev)) {
        err = PTR_ERR(shi_test_drv_dev->dev);
        pr_err("device_create err %d\n", err);
        goto out_class_destroy;
    }

    return 0;

out_class_destroy:
    class_destroy(shi_test_drv_dev->class);

out_cdev_del:
    cdev_del(&shi_test_drv_dev->cdev);

out_unregister:
    unregister_chrdev_region(shi_test_drv_dev->id, 1);

out_free:
    kfree(shi_test_drv_dev);

    return err;
}

static void __exit shi_test_drv_exit(void)
{
    pr_emerg("%s\n", __func__);

    device_destroy(shi_test_drv_dev->class, shi_test_drv_dev->id);
    class_destroy(shi_test_drv_dev->class);
    cdev_del(&shi_test_drv_dev->cdev);
    unregister_chrdev_region(shi_test_drv_dev->id, 1);
    kfree(shi_test_drv_dev);
}

module_init(shi_test_drv_init);
module_exit(shi_test_drv_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("shi test module");
