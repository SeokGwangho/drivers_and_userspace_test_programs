#pragma once
#include <vector>
#include <string>
#include <pthread.h>

// 服务器信息
#define IPADDRESS   "180.76.113.130" //
#define PORT        8888
#define LISTENQ     5

extern int PARALLEL_CLIENTS; // 并行进程数

#define MB *1024*1024
#define KB *1024
#define MAXSIZE     15 MB		// + 512 * 1024 + 64 * 1024 /*+ 8 * 1024*/ /*+ 2 * 1024*/	// 读缓冲最大字节数
#define FDSIZE      1000		// epoll 并发fd最大数目
#define EPOLLEVENTS 1000		// epoll 事件缓冲最大数目

typedef void* (*thread_func)(void*);

extern double g_conn_timeout;
extern int g_nPort;
extern pthread_t g_listen_thread;

//// server /////
void DoServer(const char* ip,int port, int backlog);

//// client ////
void DoConnect(const char* ip,int port, double sec = 0x7FFFFFFF*1.0);

static bool IsBlank(char ch)
{
	if ( ch == '\t' ) return true;
	if ( ch == ' ' ) return true;

	return false;
}
/////////////
// 命令行处理函数 //
/////////////
static std::vector<std::string> Split(std::string line)
{
	int start = -1;
	int end = -1;
	std::vector<std::string> ret;
	for (int i = 0; i < line.length(); ++i)
	{
		if ( !IsBlank(line[i]) )
		{
			if ( start == -1 ) start = i;
			end = i + 1;
		}

		if ( IsBlank(line[i]) || i == line.length() - 1 )
		{
			if ( start >= 0 )
				ret.push_back( line.substr(start, end - start) );

			start = -1;
			continue;
		}
	}

	return ret;
}
