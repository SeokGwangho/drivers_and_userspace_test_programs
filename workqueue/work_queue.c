/*
 * create_singlethread_workqueue() VS create_workqueue()
 *
 * create_singlethread_workqueue()を使用してworkqueueを作成。
 * SMPシステムの場合でも、Kernelは、1つのCPUでのworker threadのみ作成。
 *
 * create_workqueue()を使用して、SMPシステムのworkqueueを作成すると、Kernelは各CPUのworker threadを作成。
 * (Threadによって処理を並列化できるように)
 *
 *
 *
 * struct work_struct my_wq;			//
 * void my_wq_func(struct work_struct *work);	//
 *
 * INIT_WORK(&my_wq, my_wq_func);		// init and bind
 *
 * irqreturn_t xxx_interrupt(int irq, void *dev_id)
 * {
 *	...
 *	schedule_work(&my_wq);
 *	...
 *	return IRQ_HANDLED;
 * }
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

struct workqueue_struct		*my_wq;		//work queue

struct work_data {
	struct work_struct	my_work;	//work
	int	data;
};

static void work_handler(struct work_struct *work)	//pointer of my_work number
{
	struct work_data	*my_data;

	my_data = container_of(work, struct work_data, my_work);
	pr_info("%s: work data = %d\n", __func__, my_data->data);
	kfree(my_data);

	return;
}

static int __init my_workqueue_init(void)
{
	struct work_data	*my_data = NULL;

	pr_info("%s\n", __func__);

	my_data = kmalloc(sizeof(*my_data), GFP_KERNEL);
	if (!my_data)
		return -ENOMEM;

	my_data->data = 100;

	//create workqueue
	my_wq = create_singlethread_workqueue("my_single_workqueue_thread");
	if (!my_wq)
		goto	error;

	//bind work and work handler
	INIT_WORK(&my_data->my_work, work_handler);	//!= workqueue handler

	//add work to workqueue
	queue_work(my_wq, &my_data->my_work);

	return 0;

error:
	kfree(my_data);
	return -ENOMEM;
}

static void __exit my_workqueue_exit(void)
{
	return;
}

module_init(my_workqueue_init);
module_exit(my_workqueue_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("seokgh <gwangho.seok@gmail.com>");
