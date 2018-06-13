#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <arpa/inet.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <set>
#include <assert.h>
#include <signal.h>
#include <memory>
#include <sys/time.h>
#include <pthread.h>
#include <time.h>
#include <string>
#include <vector>
#include <iostream>

#include "common.h"

#define g_bPulse false		// 客户端脉冲
#define g_nPulseSec 2		// 客户端脉冲间隔

#define g_bFlood false 		// 自动刷屏
#define g_bPingpong false	// 互相弹射，客户端回弹Rebound
#define g_bBroadcast true	// 是否群发，服务端放大流量
#define g_bExcludeSelf true	// 是否不发给自己
#define g_bRetryConn false	// 连接失败，是否重试
#define g_bAcceptAlone true // 用单独的接收线程做Accept
double g_conn_timeout = 5.0;	// 连接超时，单位秒，浮点数
double g_send_timeout = 0.1;	// 连接超时，单位秒，浮点数
int PARALLEL_CLIENTS = 1;	// 并行进程数

std::set<int> g_sClients;	// 活动连接
bool g_bShow = false;		// 服务端是否显示
bool g_bBytes = true;		// 是否显示接收字节数
static int g_send_opt = 0;	// 防止服务进程无故退出的选项设置

bool g_bClientShow = !g_bPingpong;		// 客户端是否显示

int g_nPort = -1;			// 监听端口
int g_nListen = -1;			// 监听socketfd
pthread_t g_listen_thread;	// 监听线程

int g_nEpoll = -1;

void* Accept(void *arg);
class Thread
{
public:
	Thread(thread_func run_) : run(run_), bRunning(false)
	{
		if ( run ) bRunning = (pthread_create(&thread_id, NULL, run, NULL) == 0);
		if ( run == Accept ) g_listen_thread = thread_id;
	}
	// ~Thread()
	// {
	// 	if ( bRunning )
	// 	{
	// 		void * thread_ret_status = NULL;
	// 		pthread_join(thread_id, &thread_ret_status);
	// 	}
	// }
private:
	thread_func run;
	pthread_t thread_id;
	bool bRunning;
};

#define StartThread(run)	Thread __thread_##run( (run) )	// 启动线程监听端口或者stdin

void HideAll(int)
{
	g_bBytes = g_bShow;

	g_bBytes = !g_bBytes;
	g_bShow  = !g_bShow;
}

// 测试数据，固定内容的发送数据
static char send_data[MAXSIZE + 1] = {0};
static bool InitData()
{
	memset(send_data, 'a', MAXSIZE);

	system("ulimit -n 4096");

#ifndef PREVENT_SIGPIPE_QUIT
	// 以下代码，防止连接一端断开的时候，服务器进程无故退出，两种方式，可以起到相同的作用。
	if ( true ) g_send_opt = MSG_NOSIGNAL;	// 使得这个信号不会由send发出，和else分支的signal信号的作用一样，防止服务进程退出。
	else signal(SIGPIPE, SIG_IGN);	// 多个客户连接时，杀死卡死的客户端时，防止服务器被SIGPIPE信号杀死，故而忽略此信号。
	signal(SIGTRAP, HideAll);
#endif
}
static bool b_inited = InitData();
//

sockaddr_in GetAddr(const char* ip, int port)
{
	struct sockaddr_in  servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	inet_pton(AF_INET, ip, &servaddr.sin_addr);

	return servaddr;
}

int NewSock()
{
	// 新建socket
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if ( sockfd < 0) return sockfd;

	return sockfd;
}

int NewSock(const char* ip, int port)
{
	// 新建socket
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if ( sockfd < 0) return sockfd;

	// 设置连接目标
	struct sockaddr_in  servaddr = GetAddr(ip, port);

	return sockfd;
}

static void AddEvent(int epollfd, int fd, int state)
{
	struct epoll_event ev;
	ev.events = state;
	ev.data.fd = fd;
	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
}

static void DeleteEvent(int epollfd, int fd, int state)
{
	struct epoll_event ev;
	ev.events = state;
	ev.data.fd = fd;
	epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, &ev);
}

static void DoRead(int epollfd, int fd)
{
	static char buf[MAXSIZE+1] = {0};
	memset(buf, 0, sizeof(buf));

	// 接收消息
	int nread = recv(fd, buf, MAXSIZE, 0); //read(fd, buf, MAXSIZE);
	if ( g_bBytes )
		printf("%s: read %d bytes from %d, conn_num=%d\n", __func__, nread, fd, (int)g_sClients.size());

	// 读错误
	if (nread == -1)
	{
		printf("read error\n");
		perror("read error:");
		close(fd);
		DeleteEvent(epollfd, fd, EPOLLIN);

		g_sClients.erase(fd);
	}

	// 客户端挂断
	if (nread == 0)
	{
		fprintf(stderr, "client closed.\n");
		close(fd);
		DeleteEvent(epollfd, fd, EPOLLIN);

		g_sClients.erase(fd);
		return;
	}

	// 打印接收到的消息
	if ( g_bShow )
		printf("read message is : %s\n", buf);

	// static double nData = 0;
	// static double nPrev = 0;
	// int unit = 1024 * 1024 * 1024;
	// nData += nread;
	// if ( nData / unit - nPrev / unit >= 1.0 )
	// {
	// 	nPrev = nData;
	// 	printf("nData = %d G\n", (int)(nData/unit) );
	// }

	// 群发
	for (auto other : g_sClients)
	{
		if ( !g_bBroadcast && other != fd ) continue;
		if ( g_bExcludeSelf && other == fd ) continue;

		// dup traffic
		if ( !g_bPulse )
			send(other, buf, strlen(buf), g_send_opt); //write(other, buf, strlen(buf));
		//write(other, buf, strlen(buf));
	}
}

static void SetSockReuse(int listenfd)
{
	int one = 1;
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
}

static timeval GetTime(int sec, int microsec = 0)
{
	timeval tv = {sec, microsec};
	return tv;
}

static timeval GetMilliTime(int milli)
{
	if ( milli >= 1000 || milli <= 0 )
	{
		printf("milli = %d not permitted\n", milli);
		exit(-1);
	}

	timeval tv = {0, milli * 1000};
	printf("tv.s=%ld, tv.micro=%ld\n", tv.tv_sec, tv.tv_usec);
	return tv;
}

// 这个接口不调用的话，多客户端进行洪泛的时候会卡死
static void SetSockTimeout(int fd, double sec)
{
	int millis = (int)(sec * 1000);
	timeval tv = GetTime(millis/1000, millis%1000);//！！！设置为1000，曾经有过问题，如果再有问题，设置回100
	//设置发送超时，如果不设置多客户端洪泛时，会卡死
	setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(tv));
	//设置接收超时，设置与否无所谓
	//setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv));
}

static int GetSockBufLen(int fd)
{
	if ( fd < 0 ) return -1;

	int optVal = 0;

	socklen_t optLen = sizeof(int);
	getsockopt(fd, SOL_SOCKET, SO_SNDBUF, (char*)&optVal, &optLen);	// 发送socket的缓冲区 16384 接收16380

	return optVal;
}

//// server /////
static int SocketInit(const char* ip, int port, int backlog)
{
	int listenfd = NewSock(ip, port);
	if (listenfd == -1)
	{
		perror("socket error:");
		exit(1);
	}
	// 设置可重用，防止后续调试出现服务无法迅速重启的问题
	SetSockReuse(listenfd);

	printf("Buflen = %d\n", GetSockBufLen(listenfd));

	// socket绑定端口
	auto servaddr = GetAddr(ip, port);
	if( bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1 )
	{
		perror("bind error: ");
		close(listenfd);
		exit(-1);
	}

	if( listen(listenfd, backlog) < 0 )
	{
		printf("listen failed");
		close(listenfd);
		exit(-1);
	}

	return listenfd;
}

static void HandleAccept(int epollfd, int listenfd)
{
	struct sockaddr_in cliaddr;
	socklen_t  cliaddrlen;
	int clifd = accept(listenfd, (struct sockaddr*)&cliaddr, &cliaddrlen);
	if (clifd == -1)
	{
		perror("accept error:");
		return;
	}

	SetSockTimeout(clifd, g_send_timeout);

	printf("accept a new client: %s:%d\n", inet_ntoa(cliaddr.sin_addr), cliaddr.sin_port);
	
	//添加一个客户描述符和事件
	AddEvent(epollfd, clifd, EPOLLIN);
	g_sClients.insert(clifd);
}

void* Accept(void *arg)
{
	while(true)
	{
		HandleAccept(g_nEpoll, g_nListen);
	}
}

static int Connect(const char* ip, int port, double timeout_sec);

void StopListening()
{
	if ( g_bAcceptAlone )
		pthread_cancel(g_listen_thread);
	else
		DeleteEvent(g_nEpoll, g_nListen, EPOLLIN);

	close(g_nListen);

	g_nListen = -1;
	g_nPort = -1;
}

void StartListening(int new_port)
{
	g_nPort = new_port;
	g_nListen = SocketInit(IPADDRESS, g_nPort, 5);
	if ( g_bAcceptAlone )
		StartThread(Accept);
	else
		AddEvent(g_nEpoll, g_nListen, EPOLLIN);
	printf("Listening redirect to port %d\n", new_port);
}

void HandleTerm_void()
{
	std::string term_input;
	while( true )
	{
		std::getline(std::cin, term_input, '\n');

		char* buf = (char*) term_input.c_str();
		auto args = Split(buf);
		if ( args.size() == 0 ) continue;

		if ( args[0] == "clear" )
		{
			for (auto clifd : g_sClients)
			{
				close(clifd);
				DeleteEvent(g_nEpoll, clifd, EPOLLIN);
			}

			g_sClients.clear();
			continue;
		}

		if ( args[0] == "show" )
		{
			g_bShow = true;
			continue;
		}

		if ( args[0] == "hide" )
		{
			g_bShow = false;
			continue;
		}

		if ( args[0] == "bytes" )
		{
			g_bBytes = !g_bBytes;
			continue;
		}

		if ( args[0] == "list" )
		{
			printf("clients num = %d\n", (int)g_sClients.size());
			for (auto clifd : g_sClients)
			{
				printf("%d ", clifd);
			}
			if( !g_sClients.empty() ) printf("\n");

			continue;
		}

		if ( args[0] == "kick" )
		{
			if ( args.size() < 2 ) continue;

			bool bFmtErr = false;
			for ( auto ch : args[1] )
			{
				if ( ch < '0' || ch > '9')
				{
					bFmtErr = true;
					break;
				}
			}
			if ( bFmtErr )
			{
				printf("kick num format error\n");
				continue;
			}


			int nKick = atoi(args[1].c_str());

			if ( g_sClients.find(nKick) == g_sClients.end() )
			{
				printf("kicked fd %d doesn't exitst\n", nKick);
				continue;
			}

			close(nKick);
			g_sClients.erase(nKick);
			DeleteEvent(g_nEpoll, nKick, EPOLLIN);

			continue;
		}

		if ( args[0] == "connect" )
		{
			if ( args.size() < 2 )
			{
				printf("Missing arguments for connect\n");
				continue;
			}

			// 解析 ip port
			const char* ip = NULL;
			int port = -1;
			if ( args.size() >= 3 )
			{
				ip = args[1].c_str();
				port = atoi(args[2].c_str());
			}
			else if ( args.size() >=2 )
			{
				ip = IPADDRESS;
				port = atoi(args[1].c_str());
			}

			// 检查 port
			if ( port <= 0 )
			{
				printf("connect: port(%s) wrong\n", args[2].c_str() );
				continue;
			}

			int connfd = Connect(ip, port, 5);
			if (connfd < 0)
			{
				printf("connect to %s:%dfailed\n", ip, port);
				continue;
			}

			AddEvent(g_nEpoll, connfd, EPOLLIN);
			g_sClients.insert(connfd);
			printf("connected to %s:%d succeeded\n", ip, port);

			continue;
		}

		if ( args[0] == "listen" )
		{
			bool bListening = (g_nPort != -1);

			if ( args.size() == 1 )
			{
				if ( !bListening )
				{
					printf("No port listening\n");
					continue;
				}

				printf("Listening on port %d\n", g_nPort);
				continue;
			}

			// 服务端模拟时断时续的监听端口
			if ( bListening )
			{
				StopListening();
			}

			if ( args[1] != "stop" )
			{
				int new_port = atoi(args[1].c_str());
				if ( new_port <= 0 )
				{
					printf("wrong port descriptor:%s\n", args[1].c_str());
					continue;
				}

				// 重新监听
				StartListening(new_port);

				continue;
			}

			continue;
		}

		if ( strcmp(buf, "exit") == 0 || strcmp(buf, "quit") == 0 )
		{
			exit(0);
		}

		continue;
	}	
}

void* HandleTerm(void *arg)
{
	HandleTerm_void();
	return NULL;
}

void DoServer(const char* ip, int port, int backlog)
{
	g_sClients.clear();

	struct epoll_event events[EPOLLEVENTS];

	//创建一个描述符
	g_nEpoll = epoll_create(FDSIZE);

	// 初始化监听socket
	StartListening(port);

	// 单独的接收标准输入的线程
	StartThread(HandleTerm);

	while( true )
	{
		//获取已经准备好的描述符事件
		int num = epoll_wait(g_nEpoll, events, EPOLLEVENTS, -1/*((listenfd < 0)?10:20)*/ );

		for (int i = 0;i < num;i++)
		{
			int fd = events[i].data.fd;
			//根据描述符的类型和事件类型进行处理
			if ( !g_bAcceptAlone )
			 	if ( fd == g_nListen && (events[i].events & EPOLLIN))
			 		HandleAccept(g_nEpoll, g_nListen);
			if ( fd != g_nListen && (events[i].events & EPOLLIN))
				DoRead(g_nEpoll, fd);
		}
	}

	close(g_nEpoll);
	if ( g_nListen != -1 )
		close(g_nListen);
}

//// client ////
static void DoConnRead(int epollfd, int fd, int sockfd)
{
	static char buf[MAXSIZE+1] = {0};
	memset(buf, 0, sizeof(buf));

	int nread = recv(fd, buf, MAXSIZE, 0);//read(fd, buf, MAXSIZE);
	
	if (g_bBytes)
	{
		printf("%s: read %d bytes\n", __func__, nread);
	}

	timeval tv;
	gettimeofday(&tv,NULL);
	//printf( "recv time %d.%d\n", tv.tv_sec % 1000, tv.tv_usec / 1000 );

	// static double n_send = 0;
	// static double n_prev = 0;
	// int unit = 1024 * 1024 * 1024;
	// n_send += nread;
	// if ( n_send / unit - n_prev / unit >= 1.0 )
	// {
	//     printf("sent %d G\n", (int)(n_send/unit) );
	//     n_prev = n_send;
	// }

	// 读取错误
	if (nread == -1)
	{
		printf("read error\n");
		perror("read error:");
		close(fd);

		exit(-1);
	}

	// 服务端挂断
	if (nread == 0)
	{
		fprintf(stderr, "server close.\n");
		close(fd);
		exit(0);

		return;
	}

	// 打印接收到消息
	if ( g_bClientShow )
		printf("read message: %s\n", buf);

	// dup traffic
	if ( g_bPingpong)
		send(fd, buf, nread, g_send_opt);//write(fd, buf, nread);
	//write(fd, buf, nread);
}

static int Connect(const char* ip, int port, double timeout_sec)
{
	// 新建socket，绑定端口
	int sockfd = NewSock();
	if ( sockfd < 0 )
	{
		printf("error create connect socket\n");
		exit(-1);
	}
	// 设置超时，防止死锁
	SetSockTimeout(sockfd, timeout_sec);

	// 连接服务器
	auto servaddr = GetAddr(ip, port);
	if( connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0 )
	{
		close(sockfd);
		return -1;
	}

	return sockfd;
}

void HandleClientCmd(std::string cmd)
{
	auto args = Split(cmd);
	if ( args.size() == 0 ) return;

	if ( args[0] == "hide" )
	{
		g_bClientShow = false;
		return;
	}

	if ( args[0] == "show" )
	{
		g_bClientShow = true;
		return;
	}

	if ( args[0] == "bytes" )
	{
		g_bBytes = !g_bBytes;
		return;
	}
}

static int g_clientfd = -1;
void* HandleClientTerm(void* arg)
{
	std::string term_input;
	while( true )
	{
		std::getline(std::cin, term_input, '\n');
		if ( term_input[0] == ':' )
		{
			HandleClientCmd(term_input.substr(1));
			continue;
		}

		char* buf = (char*) term_input.c_str();
		send(g_clientfd, buf, strlen(buf), g_send_opt);	// 发送实际数据
	}
}

void DoConnect(const char* ip, int port, double timeout_sec)
{
	int sockfd = -1;
	clock_t start = clock();
again:
	sockfd = Connect(ip, port, timeout_sec);
	if ( sockfd < 0 )
	{
		printf("failed to connect\n");
		if ( (clock() - start) > timeout_sec*1000.0 )
		{
			printf("connect timeout %lf secs\n", timeout_sec);
			return;
		}
		if ( g_bRetryConn )
			goto again;
		else
			return;
	}

	struct epoll_event events[EPOLLEVENTS];

	// 新建epoll
	int epollfd = epoll_create(FDSIZE);

	g_clientfd = sockfd;

	// 单独的接收标准输入的线程
	StartThread(HandleClientTerm);

	// 观察标准输入和服务器socket
	AddEvent(epollfd, sockfd, EPOLLIN);
	//AddEvent(epollfd, STDIN_FILENO, EPOLLIN);

	// 自动启动刷屏
	if( g_bFlood )
	{
		printf("flood data %d start sending\n", MAXSIZE);
		//send(sockfd, send_data, MAXSIZE, g_send_opt);
		send(sockfd, send_data, MAXSIZE, g_send_opt);
		printf("flood data %d finish sending\n", MAXSIZE);
	}

	// 接收标准输入和服务器的数据
	while( true )
	{
		int num = epoll_wait(epollfd, events, EPOLLEVENTS, g_bPulse ? g_nPulseSec*1000 : -1);
		if ( g_bPulse && num == 0 )
		{
			send(sockfd, send_data, MAXSIZE, g_send_opt);
		}
		
		for (int i = 0;i < num;i++)
		{
			int fd = events[i].data.fd;
			if (events[i].events & EPOLLIN)
				DoConnRead(epollfd, fd, sockfd);
		}
	}

	close(epollfd);
	close(sockfd);
}

#if 0
		// 服务端模拟时断时续的监听端口
		if ( num == 0 )
		{
			if ( listenfd >= 0 )
			{
				printf("service shut\n");
				DeleteEvent(epollfd, listenfd, EPOLLIN);
				close(listenfd);
				listenfd = -1;
			}
			else
			{
				printf("service open\n");
				listenfd = SocketInit(ip, port, backlog);
				AddEvent(epollfd, listenfd, EPOLLIN);
			}
		}
#endif
