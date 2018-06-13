#include "common.h"

int main(int argc,char *argv[])
{
	if ( argc >= 2 )
		g_nPort = atoi(argv[1]);

	if ( g_nPort <= 0 ) g_nPort = PORT;
	printf("init port is set to %d\n", g_nPort);

    // 开始监听并处理连接
    DoServer(IPADDRESS, g_nPort, g_nPort);

    return 0;
}

