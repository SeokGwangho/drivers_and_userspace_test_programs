/***************************************************************************
test:
	ls -l /sys/kernel/test_dir
	cat /sys/kernel/test_dir/sysfs_value

	echo 123 > /sys/kernel/test_dir/sysfs_value
	cat /sys/kernel/test_dir/sysfs_value


kernel objects:
----------------------------------------
	sysfsモデルの中心は、kobjectである。
	kobjectは、sysfsとkernelをBindsする接着剤で、struct kobjectで表され、<linux /kobject.h>で定義されてる。
	struct kobjectは、sysfs fsのdirectoryとして表示されるものなど、kernel object(デバイスなど)を表す。
	Kobjectは通常、他の構造に埋め込まれてる。

	linux/kobject.h:
		#define KOBJ_NAME_LEN   20
		 
		struct kobject {
		        char                    *k_name;
		        char                    name[KOBJ_NAME_LEN];
		        struct kref             kref;
		        struct list_head        entry;
		        struct kobject          *parent;
		        struct kset             *kset;
		        struct kobj_type        *ktype;
		        struct dentry           *dentry;
		};

create a directory in /sys:
----------------------------------------
	kobject_create_and_add()を使用してdirectoryを作成できる。

		struct kobject * kobject_create_and_add(const char * name, struct kobject * parent);

	この関数は、kobject構造を動的に作成し、それをsysfsに登録する。作成できなかった場合は、NULLが返される。
	kobject構造体を使い終わったら、動的に解放するkobject_put()を呼び出す。

	Example:
		struct kobject *kobj_ref;
		 
		// creating a directory in /sys/kernel/
		kobj_ref = kobject_create_and_add("my_sysfs", kernel_kobj);	//sys/kernel/my_sysfs
		 
		// freeing kobj
		kobject_put(kobj_ref);


create sysfs file & delete :
----------------------------------------
	上記の関数を使用して、/sysにディレクトリを作成した。続いては、sysfsファイルを作成する必要がある。

	このsysファイルは、sysfsを介してUserspaceとKernelspaceをやり取りするために、使用される。
	したがって、sysfs attributes を使用して、sysfsファイルを作成できる。

	attributesは、ファイル毎に、1つの値を持つsysfsの通常のファイルとして表される。
	kobject attributesを作成するために、使用できるヘルパー関数が、ヘッダーファイルsysfs.hに沢山ある。

	struct kobj_attribute {
		struct attribute attr;
		ssize_t (*show)(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
		ssize_t (*store)(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count);
	};

	__ATTRマクロを使用して、attributeを作成できる：
		__ATTR(name, permission, show_ptr, store_ptr);


	create sysfs file:
		//create a single file attribute we are going to use:
			int sysfs_create_file(struct kobject *kobj, const struct attribute *attr);

		//create a group of attributes:
			int sysfs_create_group(struct kobject *kobj, const struct attribute_group *grp)
	delete:
		//once you have done with sysfs file, you should delete this file using:
		void sysfs_remove_file ( struct kobject *  kobj, const struct attribute * attr);

********************************************************************************/
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/slab.h>		//kmalloc()
#include <linux/uaccess.h>	//copy_to_user(),copy_from_user()
#include <linux/sysfs.h> 
#include <linux/kobject.h> 
 
dev_t dev = 0;
static struct class *dev_class;
static struct cdev sysfs_cdev;

struct kobject *kobj_ref;
volatile int sysfs_value = 0;


// This fuction will be called when we read the sysfs file
static ssize_t sysfs_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	pr_info("%s: sysfs read!\n", __func__);
	return sprintf(buf, "%d", sysfs_value);
}
 
// This fuction will be called when we write the sysfsfs file
static ssize_t sysfs_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	pr_info("%s: sysfs write!\n", __func__);
	sscanf(buf, "%d", &sysfs_value);
	return count;
}

//
struct kobj_attribute sysfs_attr = __ATTR(sysfs_value, 0660, sysfs_show, sysfs_store);



static int sysfs_open(struct inode *inode, struct file *file)
{
	pr_info("%s: device node opened!\n", __func__);
	return 0;
}
 
static int sysfs_release(struct inode *inode, struct file *file)
{
	pr_info("%s: device node closed!\n", __func__);
	return 0;
}
 
static ssize_t sysfs_read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
	pr_info("%s: device node read!\n", __func__);
	return 0;
}

static ssize_t sysfs_write(struct file *filp, const char __user *buf, size_t len, loff_t *off)
{
	pr_info("%s: device node write!\n", __func__);
	return 0;
}

static struct file_operations fops = {
	.owner          = THIS_MODULE,
	.open           = sysfs_open,
	.read           = sysfs_read,
	.write          = sysfs_write,
	.release        = sysfs_release,
};
 
static int __init sysfs_driver_init(void)
{
	/* allocating Major number */
	if((alloc_chrdev_region(&dev, 0, 1, "sysfs_Dev")) < 0) {
		pr_info("%s: cannot allocate major number!\n", __func__);
		return -1;
	}
	pr_info("%s: major = %d minor = %d\n", __func__, MAJOR(dev), MINOR(dev));
	
	/* creating cdev structure */
	cdev_init(&sysfs_cdev,&fops);
	
	/* adding character device to the system */
	if((cdev_add(&sysfs_cdev, dev, 1)) < 0) {
		pr_info("%s: cannot add the device to the system!\n", __func__);
		goto r_class;
	}
	
	/* creating struct class */
	if((dev_class = class_create(THIS_MODULE, "sysfs_class")) == NULL) {
		pr_info("%s: cannot create the class!\n", __func__);
		goto r_class;
	}
	
	/* creating device */
	if((device_create(dev_class, NULL, dev, NULL, "sysfs_device")) == NULL) {
		pr_info("%s: cannot create the device!\n", __func__);
		goto r_device;
	}
	
	/* creating a directory in /sys/kernel/ */
	kobj_ref = kobject_create_and_add("test_dir", kernel_kobj);
	
	/* creating sysfs file for sysfs_value */
	if(sysfs_create_file(kobj_ref, &sysfs_attr.attr)) {
		pr_info("%s: cannot create sysfs file......\n", __func__);
		goto r_sysfs;
	}
	pr_info("%s: device driver insert done!\n", __func__);
	return 0;

r_sysfs:
	kobject_put(kobj_ref); 
	sysfs_remove_file(kernel_kobj, &sysfs_attr.attr);
 
r_device:
	class_destroy(dev_class);
r_class:
	unregister_chrdev_region(dev,1);
	cdev_del(&sysfs_cdev);
	return -1;
}
 
static void __exit sysfs_driver_exit(void)
{
	kobject_put(kobj_ref); 
	sysfs_remove_file(kernel_kobj, &sysfs_attr.attr);
	device_destroy(dev_class, dev);
	class_destroy(dev_class);
	cdev_del(&sysfs_cdev);
	unregister_chrdev_region(dev, 1);
	pr_info("%s: device driver removw done!\n", __func__);
}
 
module_init(sysfs_driver_init);
module_exit(sysfs_driver_exit);
 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("seokgh <gwangho.seok@gmail.com>");
MODULE_DESCRIPTION("simple sysfs test driver");
