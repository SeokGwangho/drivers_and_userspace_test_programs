/*

信号可以通过 signal() 和 struct sigaction 来注册处理， 
signal()函数是 struct sigaction 中 sa_handler 的一种便捷实现。

-----------------------------------------------------------------------------------
signal()函数：
原型： void (*signal(int sig, void (*func)(int)))(int);
其中 sig 是需要捕获的 signal number, 后一个是捕获到信号后的处理函数指针，所以处理函数的原型必须是 void func(int)。

另外 api 中有下面2个特殊的 handler:
    SIG_IGN    //忽略此信号
    SIG_DFL    //恢复此信号的默认行为


sigaction.sa_flags     控制内核对该信号的处理标记
    SA_NODEFER:
		一般情况下， 当信号处理函数运行时，内核将阻塞<该给定信号 -- SIGINT>。
		但是如果设置了SA_NODEFER标记， 那么在该信号处理函数运行时，内核将不会阻塞该信号。 
		SA_NODEFER是这个标记的正式的POSIX名字(还有一个名字SA_NOMASK，为了软件的可移植性，一般不用这个名字)

    SA_RESETHAND:
		当调用信号处理函数时，将信号的处理函数重置为缺省值。
		SA_RESETHAND是这个标记的正式的POSIX名字(还有一个名字SA_ONESHOT，为了软件的可移植性，
		一般上面是对sa_flags中常用的两个值的解释
-----------------------------------------------------------------------------------
sigaction（）函数

原型： int sigaction(int sig, const struct sigaction *restrict act, struct sigaction *restrict oact);
其中 sig 为 signal number, act 指定信号的处理行为， oact 如果不为 NULL 则返回信号之前的处理行为。

struct sigaction 的主要成员如下：
 void(*) (int) 	                        sa_handler 	处理函数指针，同 signal 函数中的 func 参数
 sigset_t 	                        sa_mask 	信号屏蔽字，是指当前被阻塞的一组信号，不能被当前进程收到
 int 	                                sa_flags 	处理行为修改器，指明哪种处理函数生效，详见下文
 void(*) (int, siginfo_t *, void *) 	sa_sigaction 	处理函数指针，仅 sa_flags == SA_SIGINFO 时有效

其中 sa_flags 主要可以设置为以下值：
    SA_NOCLDSTOP	//子进程停止时不产生 SIGCHLD 信号
    SA_RESETHAND	//将信号的处理函数在处理函数的入口重置为 SIG_DFL
    SA_RESTART		//重启可中断的函数而不是给出 EINTR 错误
    SA_SIGINFO		//使用 sa_sigaction 做为信号的处理函数
    SA_NODEFER		//捕获到信号时不将它添加到信号屏蔽字中

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
