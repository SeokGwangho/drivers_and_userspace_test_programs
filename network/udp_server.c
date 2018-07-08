/*************************************************************************
 * server.c
 * 
 *************************************************************************/
#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<netdb.h>
#include<stdarg.h>
#include<string.h>


#define SERVER_PORT		8000
#define BUFFER_SIZE		1024
#define FILE_NAME_MAX_SIZE	512 

/* package header */
typedef struct {
	long int id;
	int buf_size;
	unsigned int crc32val;	/* buffer毎のcrc32値 */
	int err_flag;
} pack_info_t;

/* send package */
struct recv_pack {
	pack_info_t head;
	char buf[BUFFER_SIZE];
} data;


/*----------------------crc32----------------------*/
static unsigned int crc_table[256];
static void init_crc_table(void);
static unsigned int crc32(unsigned int crc, unsigned char * buffer, unsigned int size);
unsigned int crc = 0xffffffff;	/* 最初に渡される値は固定にする。もし発送側がこの値を使ってCRC計算するとしたら、当然受信側もこの値で計算すべき */

/**************************************************
 CRC表を初期化
 32BitのCRC表を生成する
 CRC表を生定義して、参照しても良いが、256個もあるので、生成した方が楽かも…
***************************************************/
static void init_crc_table(void)
{
	unsigned int c;
	unsigned int i, j;
	
	for (i = 0; i < 256; i++) {
		c = (unsigned int)i;
		for (j = 0; j < 8; j++) {
			if (c & 1)
				c = 0xedb88320L ^ (c >> 1);
			else
				c = c >> 1;
		}
	
		crc_table[i] = c;
	}
}

/* bufferのCRCコードを計算 */  
static unsigned int crc32(unsigned int crc,unsigned char *buffer, unsigned int size)
{
	unsigned int i;
	for (i = 0; i < size; i++) {
		crc = crc_table[(crc ^ buffer[i]) & 0xff] ^ (crc >> 8);
	}
	return crc;
}


int main(int argc, char** argv)
{
	long int send_id = 0;			/*  */
	int err_package_id = 0;			/* clientから来る */
	struct sockaddr_in server_addr;		/*  */
	int server_socket_fd;
	struct sockaddr_in client_addr;		/* client addressをGetするため用意 */
	char buffer[BUFFER_SIZE];
	char file_name[FILE_NAME_MAX_SIZE + 1];
	FILE *fp;
	int read_len = 0;
	struct timeval tv, tv2;
	fd_set read_fds, read_fds2;


	
	bzero(&server_addr, sizeof(server_addr));		/*  */
	server_addr.sin_family = AF_INET;			/*  */
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);	/*  */
	server_addr.sin_port = htons(SERVER_PORT);		/*  */
	
	/* crate socket */
	server_socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if(server_socket_fd == -1) {
		printf("[Server]create socket failed\n");
		exit(1);
	}
	
	/* bind socket */
	if(-1 == (bind(server_socket_fd,(struct sockaddr*)&server_addr, sizeof(server_addr)))) {
		printf("[Server]server bind failed\n");
		exit(1);
	}
	
	/* create crc32 table */
	init_crc_table();
	
	/* data transfer */
	while(1) {
		socklen_t client_addr_length = sizeof(client_addr);
		
		FD_ZERO(&read_fds);
		FD_SET(server_socket_fd, &read_fds);
		tv.tv_sec = 5;
		tv.tv_usec = 0;
		
		select(server_socket_fd+1, &read_fds, NULL, NULL, &tv);		/* */
		if (FD_ISSET(server_socket_fd, &read_fds)) {
			/* receive data */
			bzero(buffer, BUFFER_SIZE);
			if(recvfrom(server_socket_fd, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&client_addr, &client_addr_length) == -1) {
				printf("[Server]receive data failed\n");
				exit(1);
			}

	        	/* bufferの中からfile_nameを読みだす */
	        	bzero(file_name,FILE_NAME_MAX_SIZE + 1);
	        	strncpy(file_name, buffer, strlen(buffer) > FILE_NAME_MAX_SIZE ? FILE_NAME_MAX_SIZE : strlen(buffer));
	        	printf("[Server]%s\n", file_name);

	        	/* open the file */
	        	fp = fopen(file_name, "r");
	        	if(NULL == fp) {
		        	printf("[Server]file:%s not found\n", file_name);
	        	} else {
		        	/* Clientに送信 */
		        	while(1) {
			        	pack_info_t pack_info;
					
			        	bzero((char *)&data,sizeof(data));	/* socketの発信bufをclear */
			        	//printf("--------------------------------\n");
			        	//printf("[Server]err_package_id=%d\n", err_package_id);
			        	//printf("[Server]send_id=%ld\n", send_id);
seek:
					
				        	if((read_len = fread(data.buf, sizeof(char), BUFFER_SIZE, fp)) > 0) { 		/* read the file */
					        	data.head.id = send_id;		/* 送信idをpackageのheaderに格納 */
					        	data.head.buf_size = read_len;	/* データの長さを記録 */
					        	data.head.crc32val = crc32(crc,data.buf, sizeof(data));
					        	//printf("[Server]read_len = %d\n", read_len);
					        	//printf("[Server]data.head.crc32val = 0x%x\n", data.head.crc32val);
					        	////printf("data.buf=%s\n",data.buf);
							
				        		++send_id;
							
					        	if(sendto(server_socket_fd, (char*)&data, sizeof(data), 0, (struct sockaddr*)&client_addr, client_addr_length) < 0) {
						        	printf("[Server]send file failed\n");
						        	break;
					        	}
			        
					        	FD_ZERO(&read_fds2);
		                    			FD_SET(server_socket_fd, &read_fds2);
		                    			tv2.tv_sec = 3;
		                    			tv2.tv_usec = 0;
							
		                    			select(server_socket_fd+1, &read_fds2, NULL, NULL, &tv2);		/* */
		                    			if (FD_ISSET(server_socket_fd, &read_fds2)) { 				/* A error occurred on the client */
					                	/* receive client error package information */
					                	recvfrom(server_socket_fd, (char*)&pack_info, sizeof(pack_info), 0, (struct sockaddr*)&client_addr, &client_addr_length);
					                	err_package_id = pack_info.id;
								
					                	/* Shift to the specified location on the client */
								fseek(fp, -err_package_id, SEEK_CUR);

								goto seek;
					                	//usleep(50000);                    
		                    			} else { 								/* No error occurred on the client */
		                        			//printf("[Server]timeout again, client is busy??? I'm waiting for client...\n");
		                        			//goto resend;
		                   	 		}
				        	} else {
					        	break;
				        	}
		        	} /* end while */
			
		        	/* send the end package. '0'はclientに送信終了ってことを告げる */
		        	if(sendto(server_socket_fd, (char*)&data, 0, 0, (struct sockaddr*)&client_addr, client_addr_length) < 0) {
			        	printf("[Server]send 0 char failed\n");
			        	break;
		        	}
		
		        	printf("[Server]sever send file end 0 char\n");
		        	fclose(fp); 
		        	printf("[Server]file: %s transfer successful!\n", file_name);
				
		        	send_id = 0;	/* idをclear、次のfileの転送のために、準備 */
		        	err_package_id = 0;
	        	}
			
		} else {
			//printf("[Server]timeout, left time %ld s, %ld usec\n", tv.tv_sec, tv.tv_usec);
			printf("[Server]timeout, please input the file name on client...\n");
		}
	} /* end while */
	
	close(server_socket_fd);
	return 0;
}
