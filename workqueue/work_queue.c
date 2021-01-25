/* 
 * Workqueueの使用者側は、下記のことをやれば良い:
 * 1.workqueueを新規作成する(もし、カーネルのDefaultのworkqueueを使う場合は、ここは省略できる)
 * 2.workを新規作成する
 * 3.workをworkqueueに入れる
 * 4.schedule_work()
 * 5.flush_workqueue() & destroy_workqueue()
 *
 * workqueueを作成する方法:
 * create_singlethread_workqueue() VS create_workqueue()
 * 
 * create_singlethread_workqueue()を使用して、workqueueを作成したら、(SMPシステムの場合でも)Kernelは、1つのCPUのみでのworker threadを作成する。
 * create_workqueue()             を使用して、workqueueを作成したら、                    Kernelは、各CPUごとのworker threadを作成する。(Threadによって、処理を並列化できるように)
 *
 * A.カーネルのDefaultのworkqueueと処理threadを使って、処理を行う時に使うAPI:  	<--- Defaultのworkqueueの処理threadが重いと、処理がもっと遅くなる
 *	INIT_WORK(_work, _func, _data);
 *
 *	int schedule_work(struct work_struct *work);
 *	int schedule_delayed_work(struct work_struct *work, unsigned long delay);
 *
 *	void flush_scheduled_work(void);
 *	int cancel_delayed_work(struct work_struct *work);
 *
 * B.カーネルのDefaultのやつ(workqueueと処理thread)を使わず、自分独自のworkqueueと、独自の処理threadを新規して、処理する時に使うAPI:　　<--- 処理が上より早いはず
 * 	struct workqueue_struct *create_workqueue(const char *name);
 *
 * 	int queue_work(struct workqueue_struct *wq, struct work_struct *work);
 * 	int queue_delayed_work(struct workqueue_struct *wq, struct work_struct *work, unsigned long delay);
 *
 * 	void flush_workqueue(struct workqueue_struct *wq);
 * 	void destroy_workqueue(struct workqueue_struct *wq);
 *
 *
 * Sample:
 *
 *	 struct work_struct xxx_wq;
 *	 void xxx_do_work(struct work_struct *work);
 *
 *	 void xxx_do_work(struct work_struct *work)
 *	 {
 *		...
 *	 }
 *
 *	 irqreturn_t xxx_interrupt(int irq, void *dev_id)
 *	 {
 *	 	...
 *	 	schedule_work(&xxx_wq);			//
 *	 	...
 *	 	return IRQ_HANDLED;
 *	 }
 *
 *	 int xxx_init(void)
 *	 {
 *	 	...
 *	 	result = request_irq(xxx_irq, xxx_interrupt, "xxx", NULL);
 *	 	...
 *	 	INIT_WORK(&xxx_wq, xxx_do_work);	// init and bind
 *	 	...
 *	 }
 *
 *	 void xxx_exit(void)
 *	 {
 *	 	...
 *	 	free_irq(xxx_irq, xxx_interrupt);
 *	 	...
 *	 }
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#include <linux/smp.h>				//get_cpu(), put_cpu()
#include <asm/processor.h>			//cpu_data(), struct cpuinfo_x86


struct workqueue_struct		*my_wq;		//work queue

struct work_data {
	struct work_struct	my_work;	//work
	int	data;
};

static void pr_cpu_info(void)
{
	unsigned int cpu;				//
	//struct cpuinfo_arm64 	*info;			//
	struct cpuinfo_x86	*info;			//

	cpu = get_cpu();
	info = &cpu_data(cpu);
	pr_info("cpu=%u, cpu_core_id=%d\n", cpu, info->cpu_core_id);
	put_cpu();	//Don't forget this!

}

static void work_handler(struct work_struct *work)	//pointer of my_work number
{
	struct work_data	*my_data;

	pr_info("%s: jiffies = %ld\n", __func__, jiffies);

	pr_cpu_info();
	my_data = container_of(work, struct work_data, my_work);

	pr_info("%s: work data = %d\n", __func__, my_data->data);
	kfree(my_data);

	return;
}

static int __init my_workqueue_init(void)
{
	struct work_data	*my_data = NULL;

	pr_info("%s: module init\n", __func__);

	pr_cpu_info();

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

	pr_cpu_info();
	pr_info("%s: jiffies = %ld\n", __func__, jiffies);

	return 0;

error:
	kfree(my_data);
	return -ENOMEM;
}

static void __exit my_workqueue_exit(void)
{
	if (my_wq) {
		flush_workqueue(my_wq);
		destroy_workqueue(my_wq);
	}

	pr_info("%s: module exit\n", __func__);
	return;
}

module_init(my_workqueue_init);
module_exit(my_workqueue_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("seokgh <gwangho.seok@gmail.com>");
