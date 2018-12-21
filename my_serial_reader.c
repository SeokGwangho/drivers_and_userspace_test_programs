#include <stdio.h>
#include <getopt.h>
#include <time.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <assert.h>
#include <linux/limits.h>
#include <assert.h>
#include <ctype.h>

#define VERSION "1.00"
#define MAX_DEVICE_NAME 20
#define BUFSIZE 2048

char device[MAX_DEVICE_NAME];
int g_fd = 0;
struct termios old_pts;
unsigned int options = 0x0;
speed_t default_speed = B115200;
static const speed_t invalid_speed_parameter = (speed_t)-1;

void main_loop(int fd) {
	fd_set  ready;			/*   */
	int i = 0;
	int j = 0;
	int done = 0;
	char buf[BUFSIZE];
	struct timeval tv;
	tv.tv_usec = 0;			/* 使わないことにした */

	/****************************************************************************** 
		kernelの中でselect()の実現は実際はpollingで処理している。pollingしているfdが多いほどその分時間はかかる。

		select()に渡す引数で、次のことをKernelに指示する：
			1.対象とする記述子(fd)
			2.各記述子において、対象とする条件(記述子から読むのか、記述子に書くのか、記述子の例外条件を検出するのか)
			3.待ち時間(無期限に待つのか、一定期間待つのか、全く待たないのか)
		
		select()から戻ると、Kernelから次のことが伝えられる：
			1.準備が整った記述子のTotal数（selectでは最大1024個の記述子を監視できたっけ？epoll系新しいSystem Callで置き換え？）
			2.(読み、書き、例外条件の)3つの条件の一つが整った記述子。
		

		この戻り情報を基に、適切な入出力関数(普通はread/write)を呼べば、関数はBlockしない。


		最後の引数
			struct timeval
			{
			       time_t tv_sec;
			       time_t tv_usec;
			};
		には、3つの場合がある：

		１．NULL:
			無期限に待つことになる。Signalを捕捉すれば、この無期限の待ちに割り込める。
			指定した記述子の一つで準備ができてるか、Signalを捕捉した場合に戻る。
			Signalを捕捉した場合には、select()は−１を返し、errorにはEINTRが設定される。
			
		２．tv_sec == 0 && tv_usec == 0:
			全く待たない。指定した全ての記述子をテストし、直ちに戻る。
			これは、select()関数でBlockingせずに、複数の記述子の状態を調べる方法である。
			
		3. tv_sec != 0 || tv_usec != 0: 
			この指定した時間だけ待つ。
			指定した記述子の一つの準備が出来たか、時間切れした時に戻る。
			記述子の準備が出来る前に時間切れすると、戻り値は０である。
 	******************************************************************************/

	do { /* loop forever */
		FD_ZERO(&ready);	/* readyをクリア */
		FD_SET(fd, &ready);	/* ready変数の中で、fdが該当するbitを1にする*/
		
		select(fd+1, &ready, NULL, NULL, NULL);	/*   */
		
		/* FD_ISSETマクロでreadyの中でfdが該当するBitが1になってるかCheck。1になったら成功を意味し、read()動作に入る  */
		if (FD_ISSET(fd, &ready)) {
			i = read(fd, buf, BUFSIZE);
			if (i > 0) {
				printf("%s:\n%s", __func__, buf);
				printf("-------------------------------------\n");
			} else {
				done = 1;
			}
		}
	}  while (!done);
}

/******************************************************************************
LinuxのSerialのカノニカル｜非カノニカルの入力処理について

注意：
入力処理とは、Deviceから送信されたキャラクタを、readで読み出される前に、処理することを指す。

2種類の入力処理の中で、適切なものを選ぶべき。


「詳解UNIXプログラミング」本から：

ーーーーーーーーーーーーーーーー
種類１：　カノニカル入力処理
ーーーーーーーーーーーーーーーー
カノニカルモードは単純である。
プロセスで読み取りを行うと、端末ドライバは入力された行を返す。読み取りから戻るにはいくつかの条件がある。

条件１：指定したByte数を読み取ると戻る。一行を纏めて読む必要はない。
	行の一部を読んだ場合でも、情報を紛失することはない。
	次回の読み取りは、直前の読み取りの終了点から始まる。
条件２：行の区切りに出会うと読み取りから戻る。11.3節でカノニカルモードにおいては、NL、EOL、EOL2、EOFの文字を「行の終わり」と解釈することを述べた。
	更に、11.5節の述べたように、ICRNLを設定しIGNCRを設定してない場合、CR文字はNL文字と同様な動作をするため行を区切る。
	これらの5つの行区切りのうち、EOFは端末ドライバで処理した後廃棄される。残りの4つは行の最後の文字として呼び出し側に返される。
条件３：Signal捕捉して、関数が自動的に再Startしない場合にも、読み取りから戻る。



ーーーーーーーーーーーーーーーー
種類２：　非カノニカル入力処理
ーーーーーーーーーーーーーーーー
termios構造体のc_lflagのICANON flagをOffにすると、非カノニカルモードを指定出来る。
非カノニカルモードでは、入力データを行に纏めない。
ERASE、KILL、EOF、NL、EOL、EOL2、CR、REPRINT、STATUS、WERASEの特別な文字は処理されない。

既に述べたように、カノニカルモードは簡単である。Systemは一度に１行を返す。しかし、非カノニカルモードでは、
SystemはProcessにデータを返す時期をどのように知るのであろうか？
一度に１Byteを返すと、非常に大きなOverheadを伴う(勿論、一度に２Byteを返すと、Overheadは半分に減る)。
つまり、読み取りを始める前に、何Byte読めるか分からないため、Systemが常に複数のByteを返すことが出来ないのだ。

解決策は、
	指定したデータ量を読み取った場合、或は
	指定した時間が経過した場合に、戻るようにSystemに設定することである。
	
	これには、termios構造体の配列c_cc[]の中の２つの変数：MIN,TIME(index: VMIN,VTIME)を使う。
	MINは、readから戻るまで最小Byte数を指定する。
	TIMEは、データの到着の待ち時間を1/10秒単位で指定する。

	なので、MIN、TIMEを使う時には、４パターンがある：
	場合A： MIN>0,TIME>0
	場合B： MIN>0,TIME=0
	場合C： MIN=0,TIME>0
	場合D： MIN=0,TIME=0

               MIN>0                                 MIN=0
         +---------------------------------+---------------------------------------------+
         |pattern A:                       |  pattern C:                                 |
         |  Timerが切れる前にread()は      |    Timerが切れる前にread()は[1,nbytes]を返す|
TIME>0   |  [MIN,nbytes]を返す。           |    Timerが切れるとread()は0を返す。         |
         |  Timerが切れると[1,MIN]を返す。 |    (タイマーはreadタイマー)                 |
         |  Timerはバイトタイマー          |                                             |
         |  呼び出し側無期限Blockされる。  |                                             |
         +-------------------------------------------------------------------------------+
         |pattern B:                       |  pattenr D:                                 |
TIME=0   |  データがあればread()は         |                                             |
         |    [MIN,nbytes]を返す           |    read()は直ちに[0,nbytes]を返す           |
         | (呼び出し側は無期限Blockされる) |                                             |
         +---------------------------------+---------------------------------------------+



ーーーーーーーーーーーーーーーー
非同期入力
ーーーーーーーーーーーーーーーー
上記2つのモードは、更に、同期/非同期で使うことができるが、

Defaultは、入力が上手く行くまで、read()がBlockされる同期モードである。

非同期モードでは、read()は即座に終了し、
後で読み込みが完了した時に、プログラムにSignalが送られる。
このSignalは、Signal　Handlerを使って受け取るべき。

******************************************************************************/
void init_com_port(struct termios *pts)
{
	/*   */
	pts->c_lflag &= ~ICANON;			/*   */
	pts->c_lflag &= ~(ECHO | ECHOCTL | ECHONL);	/*   */

	pts->c_cflag |= HUPCL;				/*   */
	pts->c_cflag &= ~CRTSCTS;			/*   */

	pts->c_cc[VMIN] = 1;				/*   */
	pts->c_cc[VTIME] = 0;				/*   */

	pts->c_iflag &= ~ICRNL;				/*   */
	pts->c_iflag &= ~(IXON | IXOFF | IXANY);	/*   */

	pts->c_oflag &= ~ONLCR; 			/*   */

	/* set hardware flow control by default */
	//pts->c_cflag |= CRTSCTS;
	//pts->c_iflag &= ~(IXON | IXOFF | IXANY);

	cfsetospeed(pts, default_speed);
	cfsetispeed(pts, default_speed);
}

static const struct option longopts[] = {
	{"device", required_argument, NULL, 'd'},
	{"speed", required_argument, NULL, 's'},
	{"help", no_argument, NULL, 'h'},
	{"version", no_argument, NULL, 'v'},
	{NULL, 0, NULL, 0}
};

static void help(FILE *output)
{
	fprintf(output, "\nUsage: serial_reader [options]\n"
		" [options]\n"
		"\t-d<devfile>, --device=<devfile>\n"
		"\t\toption example: -d /dev/ttymxc2\n"
		"\t-s,--speed=<speed>\n"
		"\t-h,--help\n"
		"\t-v,--version\n"
		"\n");
}

static inline void main_version(void)
{
	printf("serial_reader v"VERSION"\n");
	exit(0);
}

#define STRING(x) #x
#define TO_STRING(x) STRING(x)
#define S(x)    X(x,B##x,TO_STRING(x))
#define SPEED_TABLE \
	S(0) \
	S(4800) \
	S(9600) \
	S(19200) \
	S(38400) \
	S(57600) \
	S(115200) 

static inline speed_t check_speed_parameter(const char* value)
{
	speed_t speed = invalid_speed_parameter;
	const unsigned long int requested_speed = strtoul(value, NULL,0);
	switch(requested_speed) {
#define X(x,y,z) case x: return y;
	SPEED_TABLE
	default:
        return invalid_speed_parameter;
#undef X
	}
}

static inline const char* valid_terminal_speeds()
{
#define X(x,y,z) "\n\t -" z
	return SPEED_TABLE;
#undef X
}

#undef SPEED_TABLE
#undef S

static inline int parse_cmdline(int argc, char *argv[])
{
	int error = EXIT_SUCCESS;
	int optc;

	while (((optc = getopt_long (argc, argv, "d:s:hv", longopts, NULL)) != -1) && (EXIT_SUCCESS == error)) {
		int param;
		switch (optc) {
		case 'd':
		    strncpy(device, optarg, MAX_DEVICE_NAME);
		    break;
		case 's': {
		    const speed_t requested_speed = check_speed_parameter(optarg);
		    if (requested_speed != invalid_speed_parameter) {
			default_speed = requested_speed;
		    } else {
			fprintf(stderr,"invalid speed parameters arguments %s, valid values are %s\n",optarg,valid_terminal_speeds());
		    }
		}
		break;
		case 'h':
		    help(stdout);
		    exit(error);
		    break;
		case 'v':
		    main_version();
		    break;
		case '?':
		    error = EINVAL;
		    exit(error);
		    break;
		default:
		    error = EINVAL;
		    fprintf(stderr,"invalid argument (%d)\n",optc);
		    help(stderr);
		    exit(error);
		    break;
		}
	}

	return error;
}

void signal_handler(int signal)
{
	tcsetattr(g_fd, TCSANOW, &old_pts);
	exit(EXIT_SUCCESS);
}

int send_command(char *cmd)
{
	int ret = 0;
	int resp_bytes = 0;
	char resp_buffer[20] = {0};
	char *ptr = NULL;

	printf("send command: %s", cmd);
	if (write(g_fd, cmd, strlen(cmd)) < 0) {
		perror("write /dev/ttymxc2 error, exit!\n");
		return -1;
	} else {
retry:
		/* check response */
		resp_bytes = read(g_fd, resp_buffer, sizeof(resp_buffer));
		if (-1 == resp_bytes) {
			perror("can't read response, retry!\n");
			goto retry;
		} else {
			//printf("read %d bytes response data:%s", resp_bytes, resp_buffer);
			ptr = strstr(resp_buffer, "Done");
			if ( ptr == NULL) {
				goto retry;
			} else {
				printf("send command success!\n\n");
				ret = 0;
			}
		}
	}
	return ret;
}


int main(int argc, char *argv[])
{
	struct termios pts;
	struct sigaction sig_act;
	int error = EXIT_SUCCESS;

	device[0] = '\0';

	error = parse_cmdline(argc,argv);

	g_fd = open(device, O_RDWR);
	if (g_fd < 0) {
		const int error = errno;
		printf("cannot open device %s, error = %d, exit!\n", device, error);
		return -1;
	}

	/* modify the port configuration */
	tcgetattr(g_fd, &pts);
	memcpy(&old_pts, &pts, sizeof(old_pts));
	init_com_port(&pts);
	tcsetattr(g_fd, TCSANOW, &pts);


	/* register the signal handler */
	sig_act.sa_handler = signal_handler;
	sigaction(SIGHUP, &sig_act, NULL);
	sigaction(SIGINT, &sig_act, NULL);
	sigaction(SIGPIPE, &sig_act, NULL);
	sigaction(SIGTERM, &sig_act, NULL);

	/* run the main loop */
	main_loop(g_fd);

	exit(error);
}

