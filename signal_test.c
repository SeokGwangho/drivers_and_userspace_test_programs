/*
sigaction.sa_flags     控制内核对该信号的处理标记
    SA_NODEFER:
		一般情况下， 当信号处理函数运行时，内核将阻塞<该给定信号 -- SIGINT>。
		但是如果设置了SA_NODEFER标记， 那么在该信号处理函数运行时，内核将不会阻塞该信号。 
		SA_NODEFER是这个标记的正式的POSIX名字(还有一个名字SA_NOMASK，为了软件的可移植性，一般不用这个名字)

    SA_RESETHAND:
		当调用信号处理函数时，将信号的处理函数重置为缺省值。
		SA_RESETHAND是这个标记的正式的POSIX名字(还有一个名字SA_ONESHOT，为了软件的可移植性，
		一般上面是对sa_flags中常用的两个值的解释

*/

#include <stdio.h>
#include <signal.h>
#include <unistd.h>

void show_handler(int sig)
{
    printf("%s got signal %d\n", __func__, sig);
}

int main(int argc, char **argv)
{
    int i = 0;
    struct sigaction act, oldact;

    act.sa_handler = show_handler;
    sigaddset(&act.sa_mask, SIGQUIT);

    act.sa_flags = /* SA_RESETHAND | */ SA_NODEFER;
    act.sa_flags = 0;

    sigaction(SIGHUP, &act, &oldact);
    sigaction(SIGINT, &act, &oldact);
    sigaction(SIGQUIT, &act, &oldact);
  
    // 或者不使用sigaction结构体的方式，直接用signal()系统调用
    //signal(SIGHUP,  show_handler);
    //signal(SIGINT,  show_handler);
    //signal(SIGQUIT, show_handler);

    while(1) {
        sleep(1);
        printf("pid %d sleeping %ds\n", getpid(), i);
        i++;
    }
}
