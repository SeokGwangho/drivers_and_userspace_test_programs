/*
 * tasklet test driver
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>

char tasklet_data[] = "this is tasklet example data";

static void do_tasklet(unsigned long data)
{
	pr_info("%s: data = %s\n", __func__, (char *)data);

	return;
}

DECLARE_TASKLET(my_tasklet, do_tasklet, (unsigned long)tasklet_data);

static int __init my_tasklet_init(void)
{
	pr_info("%s\n", __func__);

	tasklet_schedule(&my_tasklet);

	return 0;
}

static void __exit my_tasklet_exit(void)
{
	pr_info("%s\n", __func__);

	tasklet_kill(&my_tasklet);

	return;
}

module_init(my_tasklet_init);
module_exit(my_tasklet_exit);
MODULE_AUTHOR("seokgh <gwangho.seok@gmail.com>");
MODULE_LICENSE("GPL");
