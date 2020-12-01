/********************************************************************************
*  ex:
* 	termimal1:
*               sudo cat /proc/kmsg   or dmesg
* 	termimal2:
*		sudo insmod hello_world.ko g_name="seokgh" g_val=100 g_array=1,2,3,4
* 	termimal3:
*		sudo sh -c "echo 87 > /sys/module/hello_world_module/parameters/g_cb_val"
* 	or
*		sudo cat /sys/module/hello_world_module/parameters/g_name 
*		sudo cat /sys/module/hello_world_module/parameters/g_array 
* *******************************************************************************/
#include<linux/kernel.h>
#include<linux/init.h>
#include<linux/module.h>
#include<linux/moduleparam.h>	//must
 
char *g_name;
int g_val, g_array[4];
int g_cb_val;
 
module_param(g_val, int, S_IRUSR|S_IWUSR);			//integer value
module_param(g_name, charp, S_IRUSR|S_IWUSR);			//String
module_param_array(g_array, int, NULL, S_IRUSR|S_IWUSR);	//Array of integers

int notify_param(const char *val, const struct kernel_param *kp)//for Module_param_cb()
{
	int res = param_set_int(val, kp);			// Use helper for write variable
	if(res == 0) {
		pr_info("%s: call back function called... the new g_cb_val=%d\n", __func__, g_cb_val);
                return 0;
	}

	return -1;
}
 
const struct kernel_param_ops my_param_ops = {			//for Module_param_cb()
	.set = &notify_param,
	.get = &param_get_int,
};
 
module_param_cb(g_cb_val, &my_param_ops, &g_cb_val, S_IRUGO|S_IWUSR );	//for Module_param_cb()
 

static int __init hello_world_init(void)
{
	int i;

	pr_info("%s: g_name = %s, g_val = %d, g_cb_val = %d\n", __func__, g_name, g_val, g_cb_val);
	for (i = 0; i < (sizeof g_array / sizeof (int)); i++) {
		pr_info("%s: g_array[%d] = %d\n", __func__, i, g_array[i]);
	}
	pr_info("%s: kernel module inserted successfully...\n", __func__);
	return 0;
}
 
static void __exit hello_world_exit(void)
{
	pr_info("%s: kernel module removed successfully...\n", __func__);
}
 
module_init(hello_world_init);
module_exit(hello_world_exit);
 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("seokgh <gwangho.seokgmail.com>");
MODULE_DESCRIPTION("A simple hello world driver");
MODULE_VERSION("0.0");
