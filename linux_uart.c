#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <assert.h>
#include <unistd.h>

#define MAX_SCRIPT_NAME 20 /* maximum length of the name of the script file */
#define MAX_DEVICE_NAME 20 /* maximum length of the name of the /dev comm port driver */

#define BUFSIZE 1024
#define byte unsigned char
#define TRUE 1
#define FALSE 0

typedef enum {
  S_TIMEOUT,		/* timeout */
  S_DTE,		/* incoming data coming from kbd */
  S_DCE,		/* incoming data from serial port */
  S_MAX			/* not used - just for checking */
} S_ORIGINATOR;



/*typedef to represent the current line state

warning: using int for baud_rate is not  safe in 16-bit/8-bit architectures,
fine in 32/64-bit including i386+, alpha, mips, ia64, amd64, arm,sparc etc*/ 

typedef struct {
    int baud_rate; /*300,1200,2400,4800,9600,19200,38400,57600,115200*/
    byte data_bits; /*7 or 8*/
    char parity; /*e for even, o for odd and n for none*/
    byte stop_bits; /*1 or 2*/
    char flow_control; /*h for hardware, s for software, n for none*/
    char echo_type; /*r for remote, l for local and n for none*/
} state;
state curr_state;



int crnl_mapping; //0 - no mapping, 1 mapping

char device[MAX_DEVICE_NAME]; /* serial device name */

int  pf = 0;  /* port file descriptor */
struct termios pots; /* old port termios settings to restore */
struct termios sots; /* old stdout/in termios settings to restore */


static int help_state = 0;
static int in_escape = 0;

char err[] = "\nInvalid option\n";
 char leaving[]="\n\nLeaving menu\n\n";


/******************************************************************
menu functions

all functions of the naming convention xxxx_menu just display a menu
all functions of the naming convention process_xxxx_menu perform the actions for that menu

menu is called by pressing ctrl+s, handled in cook_buf()
*/


/*Display a custom message, usage screen and exit with a return code of 1*/
void usage(char *str) {
    fprintf(stderr,"%s\n",str);    
    fprintf(stderr, "Usage: nanocom device [options]\n\n"
            "\tdevice\t\tSpecifies which serial port device to use, on most systems this will be /dev/ttyS0 or /dev/ttyS1\n\n" 
            "\t[options] All options are required\n\n"
            "\t-b Bit rate, the bit rate to use, valid options are 300, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200.\n"
            "\t-p Parity setting, n for none, e for even or o for odd. Must be the same as the remote system.\n"
            "\t-s Stop bits, the number of stop bits (1 or 2)\n"
            "\t-d Data bits, the nuimber of data bits (7 or 8)\n"
            "\t-f Flow control setting, h for hardware (rts/cts), s for software (Xon/xoff) or n for none.\n"
            "\t-e Echo setting\n"
            "\t\tl Local echo (echos everything you type, use if you don't see what you type)\n"
            "\t\tr Remote echo (use if remote system doesn't see what is typed OR enable local echo on remote system)\n"
            "\t\tn No echoing.\n\n");         
  exit(1);
}


/* restore original terminal settings and exit */
void cleanup_termios(int signal) {
  
  /*flush any buffers*/
  tcflush(pf,TCIFLUSH); 
  
  /*restore settings*/
  tcsetattr(pf, TCSANOW, &pots);
  tcsetattr(STDIN_FILENO, TCSANOW, &sots);
  exit(0);
} 


/*displays the current terminal settings*/
void display_state() {
    fprintf(stderr,"\n****************************************Line Status***********************************************\n"
   "%d bps\t %c Data Bits\t %c Parity \t %c Stop Bits \t %c Flow Control %c echo\n"
   "**************************************************************************************************\n\n",
   curr_state.baud_rate,curr_state.data_bits,curr_state.parity,curr_state.stop_bits,curr_state.flow_control,curr_state.echo_type);
}

/*resets the menu state so we aren't left in a deeper level of the menu*/
static void reset_state(void) {
    in_escape=-1;
    help_state=-1;
    write(STDOUT_FILENO,leaving,strlen(leaving));
}
   

/*the main menu*/
void main_menu(void) {

   char str1[] = "**********Menu***********\n"
    "1. Display Current Line Status\n"
    "2. Set Bit Rate\n"
    "3. Set Data Bits\n"
    "4. Set Parity\n"
    "5. Set Stop Bits\n"
    "6. Set Flow Control\n"
    "7. Set Echo Settings\n"
    "8. Exit\n"
    "9. Leave Menu\n"
    "*************************\n";   

  write(STDOUT_FILENO, str1, strlen(str1)); 
}

/*display baud rate menu*/
static void baud_menu(void) {
  char str[] =
    "\n******Set speed *********\n"
    " a - 300\n"
    " b - 1200\n"
    " c - 2400\n"
    " d - 4800\n"
    " e - 9600\n"
    " f - 19200\n"
    " g - 38400\n"
    " h - 57600\n"
    " i - 115200\n"
    "*************************\n"
    "Command: ";

  write(STDOUT_FILENO, str, strlen(str));
}

/*display parity menu*/
static void parity_menu(void) {
  char str[] =
    "\n******Set Pairty*********\n"
    " n - No Parity\n"
    " e - Even Parity\n"
    " o - Odd Parity\n"
    "*************************\n"
    "Command: ";

  write(STDOUT_FILENO, str, strlen(str));
}

/*display data bits menu*/
static void data_bits_menu(void) {
    char str[] =
    "\n******Set Number of Data Bits*********\n"
    " 7 - 7 Data Bits\n"
    " 8 - 8 Data bits\n"
    "*************************\n"
    "Command: ";

  write(STDOUT_FILENO, str, strlen(str));
}

/*display stop bits menu*/
static void stop_bits_menu(void) {
    char str[] =
    "\n******Set Number of Stop Bits*********\n"
    " 1 - 1 Stop Bit\n"
    " 2 - 2 Data Bits\n"
    "*************************\n"
    "Command: ";

  write(STDOUT_FILENO, str, strlen(str));
}

/*display menu for flow control*/
static void flow_control_menu(void) {
    char str[] =
    "\n******Set Flow Control Type*********\n"
    " h - Hardware Flow Control (CTS/RTS)\n"
    " s - Software Flow Control (XON/XOFF)\n"
    " n - No Flow Control\n"
    "*************************\n"
    "Command: ";

  write(STDOUT_FILENO, str, strlen(str));
}

/*display menu for echo settings*/
static void echo_settings_menu(void) {
    char str[] =
    "\n******Set Echo Type*********\n"
    " l - local echo (echo what you type locally)\n"
    " r - remote echo (echo what a remote system sends you, back to that system)\n"
    " n - No Echoing\n"
    "*************************\n"
    "Command: ";

  write(STDOUT_FILENO, str, strlen(str));
}


/* process function for help_state=0 */
static void process_main_menu(int fd, char c) { 
  

  

  help_state = c - 48;
  
  switch (c) {
    /*display current settings*/
    case '1':
        display_state();
        reset_state();
        /*we want to exit the menu*/
        break;
    /*baud rate*/
    case '2':
        /*display baud rate menu*/
        baud_menu();
        break;
    /*data bits*/
    case '3':
        data_bits_menu();
        break;
    /*parity*/
    case '4':
        parity_menu();
        break;
    /*stop bits*/
    case '5':
        stop_bits_menu();
        break;  
   /*flow control*/
    case '6':
        flow_control_menu();
        break;
    /*echo settings*/
    case '7':
        echo_settings_menu();
        break;
    /*quit the program*/
    case '8':
      /* restore termios settings and exit */
      write(STDOUT_FILENO, "\n", 1);
      cleanup_termios(0);
      break;
  case '9': /* quit help */
      reset_state();
      break;

    
  default:
    /* pass the character through */
    /* "C-\ C-\" sends "C-\" */
    write(fd, &c, 1);
    break;
  }



}

/* setup the terminal according to settings in curr_state*/
void init_comm() {
    struct termios pts;
    tcgetattr(pf, &pts);
    /*make sure pts is all blank and no residual values set
    safer than modifying current settings*/
    pts.c_lflag=0; /*implies non-canoical mode*/
    pts.c_iflag=0;
    pts.c_oflag=0;
    pts.c_cflag=0;
  
    switch (curr_state.baud_rate)
    {
        case 300:
             pts.c_cflag |= B300;
            break;
        case 1200:
            pts.c_cflag |= B1200;
            break;
        case 2400:
            pts.c_cflag |= B2400;
            break;
        case 4800:
            pts.c_cflag |= B4800;
            break;
        case 9600:
            pts.c_cflag |= B9600;
            break;
        case 19200:
            pts.c_cflag |= B19200;
            break;
        case 38400:
            pts.c_cflag |= B38400;
            break;
        case 57600:
            pts.c_cflag |= B57600;
            break;
        case 115200:
            pts.c_cflag |= B115200;
            break;
        default:
            usage("ERROR: Baud rate must be 300, 1200, 2400, 4800, 9600, 19200, 38400, 57600 or 115200");
            break;
    }           
   
    switch (curr_state.data_bits)
    {
        case '7':
            pts.c_cflag |= CS7;        
            break;
        case '8':
            pts.c_cflag |= CS8;  
            break;
       default:
           usage("ERROR: Only 7 or 8 databits are allowed");
           break;
    }
    
    switch (curr_state.stop_bits) {
    
        case '1':
           /*1 stop bit, default setting no action required*/
           break;
           
        case '2':
           /*2 stop bits*/
           pts.c_cflag |= CSTOPB;            
           break;
           
        default:
           usage("ERROR: Stop bit must contain a value of 1 or 2");
           break;
    }
    
    switch(curr_state.flow_control)
    {
        /*hardware flow control*/
        case 'h':
            pts.c_cflag |= CRTSCTS;      
            break;
    
        /*software flow control*/
        case 's':
            pts.c_iflag |= IXON | IXOFF | IXANY;               
            break;
        
        /*no flow control*/
        case 'n':        
            break;
            
        default:
            usage("ERROR: Flow control must be either h for hardware, s for software or n for none");
            break;
    }    
    
    /*ignore modem lines like hangup*/
    pts.c_cflag |= CLOCAL;
    /*let us read from this device!*/
    pts.c_cflag |= CREAD;
    
    switch (curr_state.parity)  {
        case 'n':
            /*turn off parity*/ 
            break;
            
        case 'e':
            /*turn on parity checking on input*/
            pts.c_iflag |= INPCK;
 
            /*set parity on the cflag*/
            pts.c_cflag |= PARENB;
            
            /*even parity default, no  need to set it*/       
            break;
            
        case 'o':
            /*turn on parity checking on input*/
            pts.c_iflag |= INPCK;
            
            /*set parity on the cflag*/
            pts.c_cflag |= PARENB;
            
            /*set parity to odd*/
            pts.c_cflag |= PARODD;
            break;
            
        default:
            usage("Error: Parity must be set to n (none), e (even) or o (odd)");
            break;
    }
    
    switch(curr_state.echo_type)
    {
        case 'r':
            pts.c_lflag |= (ECHO | ECHOCTL | ECHONL);  
            break;
        case 'n':
            /*handled in init_stdin*/
            break;
        case 'l':
            break;
        default:
            usage("Error: Echo type must be l (local), r (remote) or n (none)");
            break;
    }      
    

    
    
    /*perform newline mapping, useful when dealing with dos/windows systems*/
    pts.c_oflag = OPOST;
  
   /*pay attention to hangup line*/
   pts.c_cflag |= HUPCL;
   
   /*don't wait between receiving characters*/
   pts.c_cc[VMIN] = 1;
   pts.c_cc[VTIME] = 0;
   

    /*translate incoming CR to LF, helps with windows/dos clients*/
    pts.c_iflag |= ICRNL;
    /*translate outgoing CR to CRLF*/
   pts.c_oflag |= ONLCR;
   crnl_mapping = 1;

   /*set new attributes*/
   tcsetattr(pf, TCSANOW, &pts);  
}


/*process responses to select a new baud rate*/
static void process_baud_menu(int fd,char c) {
    switch(c) {
        case 'a':
            curr_state.baud_rate=300;
            init_comm();
            break;
        case 'b':
            curr_state.baud_rate=1200;
            init_comm();
            break;
        case 'c':
            curr_state.baud_rate=2400;
            init_comm();            
            break;
        case 'd':    
            curr_state.baud_rate=4800;
            init_comm();       
            break;
        case 'e':
            curr_state.baud_rate=9600;
            init_comm();  
            break;
        case 'f':         
            curr_state.baud_rate=19200;
            init_comm();  
            break;
        case 'g':
            curr_state.baud_rate=38400;
            init_comm();  
            break;       
        case 'h':
            curr_state.baud_rate=57600;
            init_comm();            
            break;       
        case 'i':
            curr_state.baud_rate=115200;
            init_comm();                        
            break;       
        default:
           write(STDOUT_FILENO, err, strlen(err));
           break;       
    }
            
} 

/*process responses to select number of data bits*/
static void process_data_bits_menu(int fd,char c) {
    switch(c) {
        case '7':
        case '8':
            curr_state.data_bits=c;
            init_comm();
            break;
       default:
           write(STDOUT_FILENO, err, strlen(err));
           break;
    }            
} 
  
/*process responses to select parity settings*/
static void process_parity_menu(int fd,char c) {
    switch(c) {
        case 'n':
        case 'e':
        case 'o':
            curr_state.parity=c;
            init_comm();
            break;
       default:
           write(STDOUT_FILENO, err, strlen(err));
           break;
    }
            
} 

/*process responses to select number of stop bits*/
static void process_stop_bits_menu(int fd,char c) {
    switch(c) {
        case '1':
        case '2':
            curr_state.stop_bits=c;
            init_comm();
            break;
       default:
           write(STDOUT_FILENO, err, strlen(err));
           break;
    }            
} 
  
/*process responses to select flow control type*/
static void process_flow_control_menu(int fd,char c) {
    switch(c) {
        case 'n':
        case 'h':
        case 's':
            curr_state.flow_control=c;
            init_comm();
            break;
       default:
           write(STDOUT_FILENO, err, strlen(err));
           break;
    }            
} 
  
/*process responses to select echo settings*/
static void process_echo_settings_menu(int fd,char c) {
    switch(c) {
        case 'n':
        case 'l':
        case 'r':
            curr_state.echo_type=c;
            init_comm();
            break;
       default:
           write(STDOUT_FILENO, err, strlen(err));
           break;
    }            
} 
  


/*launch the right help processor based on the current help state
*/
static void parse_help_state(int fd,char buf)
{
        switch (help_state) {
            case 0:
                process_main_menu(fd, buf);
                break;
            /*there is no need for case 1 as it just displays the settings, no input required*/                
            case 2:
                process_baud_menu(fd,buf);
                reset_state();  
                break;
            case 3:
                process_data_bits_menu(fd,buf);
                reset_state();
                break;
            case 4:
                process_parity_menu(fd,buf);
                reset_state();
                break;
            case 5:
                process_stop_bits_menu(fd,buf);
                reset_state();
                break;
            case 6:
                process_flow_control_menu(fd,buf);
                reset_state();
                break;
           case 7:
                process_echo_settings_menu(fd,buf);
                reset_state();
                break;
            /*there is no need for case 8 as we'll have exited before getting to this menu*/
            /*same goes for case 9*/
           default: 
               reset_state();
               break;
        } 
}


/*void cook_buf(int fd, char *buf, int num) -
    redirect escaped characters to the help handling functions;
    the function is called from mux.c in the main character
    processing ruoutine;
    
    - fd - file handle for the comm port
    - buf - pointer to the character buffer
    - num - number of valid characters in the buffer*/

void cook_buf(int fd, char *buf, int num) { 
  int current = 0;
  
  if (in_escape) {
    /* cook_buf last called with an incomplete escape sequence */
    parse_help_state(fd,buf[current]);
    num--; /* advance to the next character in the buffer */
    buf++; 
  }

  while (current < num) { /* big while loop, to process all the charactes in buffer */

    /* look for the next escape character 'ctrl+s' */
    while ((current < num) && (buf[current] != 0x14)) {
        current++;
    }
    /* and write the sequence befor esc char to the comm port */
    if (current) {
        write (fd, buf, current);
    }

    if (current < num) { /* process an escape sequence */
        /* found an escape character */
        if (help_state == 0) {
            /*not already in help, display the menu*/
            main_menu();
        }
        current++;
        if (current >= num) {
            /* interpret first character of next sequence */
            in_escape = 1;
            return;
        }
        parse_help_state(fd,buf[current]);

        /* end switch */
        current++;
        if (current >= num) {      
            return;      
        }
    } /* if - end of processing escape sequence */
    num -= current;
    buf += current;
    current = 0;
  } /* while - end of processing all the charactes in the buffer */
  return;
}


void init_stdin(struct termios *sts) {
   /* again, some arbitrary things */
   sts->c_iflag &= ~BRKINT;
   sts->c_iflag |= IGNBRK;
   sts->c_lflag &= ~ISIG;
   sts->c_cc[VMIN] = 1;
   sts->c_cc[VTIME] = 0;
   sts->c_lflag &= ~ICANON;
   
   
   /*enable local echo:*/

   if (curr_state.echo_type=='l')
   {
       sts->c_lflag |= (ECHO | ECHOCTL | ECHONL);
   }
   else
   {
       sts->c_lflag &= ~(ECHO | ECHOCTL | ECHONL);
   }
}

/* main program loop */
void mux_loop(int pf) {
  fd_set  ready;        /* used for select */
  int i = 0;        /* used in the multiplex loop */
  int done = 0;
  char buf[BUFSIZE];
  struct timeval tv;
  tv.tv_usec = 0;

    do { /* loop forever */
        FD_ZERO(&ready);
        FD_SET(STDIN_FILENO, &ready);
        FD_SET(pf, &ready);
    
        select(pf+1, &ready, NULL, NULL, NULL);
    
        if (FD_ISSET(pf, &ready)) {
            /* pf has characters for us */
            i = read(pf, buf, BUFSIZE);
            
            if (i > 0) {
                write(STDOUT_FILENO, buf, i);
            }
            else {
                done = 1;
            }
        } /* if */
    
        if (FD_ISSET(STDIN_FILENO, &ready)) {
                /* standard input has characters for us */
                i = read(STDIN_FILENO, buf, BUFSIZE);
            if (i > 0) {
                cook_buf(pf, buf, i);
            }
            else {
                done = 1;
            }
        } /* if */
    }  while (!done); /* do */
}





/*
*******************************************************************
Main functions
********************************************************************
*/



int main(int argc, char *argv[]) {
    struct  termios pts;  /* termios settings on port */
    struct  termios sts;  /* termios settings on stdout/in */
    struct sigaction sact;/* used to initialize the signal handler */
    int i;
    int opt;
    
    device[0] = '\0';
    
    /* open the device */
    pf = open(argv[1], O_RDWR);
    if (pf < 0) {
        usage("Cannot open device");
    }
        
    /*get current device attributes*/
    tcgetattr(pf, &pts);      
        
    while ( (opt = getopt(argc,argv,"b:p:s:d:f:e:")) >0) {
        switch (opt)  {
            /*set the baud rate*/
            case 'b':
                
                curr_state.baud_rate=atoi(optarg);              
                break;     
            /*setup flow control*/
            case 'f':
                curr_state.flow_control=optarg[0];               
                break;
                
            /*stop bits*/
            case 's':
                curr_state.stop_bits=optarg[0];
                break;    
                
            /*parity*/
            case 'p':
                curr_state.parity=optarg[0];               
                break;           
            
            /*data bits*/ 
            case 'd':
                curr_state.data_bits=optarg[0];
                break;
                
           /*echo type*/
           case 'e':
               curr_state.echo_type=optarg[0];
               break;
                                       
            default:
                usage("");
                break; 
        }/*end switch*/
    }  /*end while*/

    /*save old serial port config*/
    tcgetattr(pf, &pts);
    memcpy(&pots, &pts, sizeof(pots));
    
    /*setup serial port with new settings*/
    init_comm(&pts);
        
    fprintf(stderr,"Press CTRL+T for menu options");   
    display_state();
     
        
    /* Now deal with the local terminal side */
    tcgetattr(STDIN_FILENO, &sts);
    memcpy(&sots, &sts, sizeof(sots)); /* to be used upon exit */
    init_stdin(&sts);
    tcsetattr(STDIN_FILENO, TCSANOW, &sts);
    
    /* set the signal handler to restore the old
    * termios handler */
    sact.sa_handler = cleanup_termios; 
    sigaction(SIGHUP, &sact, NULL);
    sigaction(SIGINT, &sact, NULL);
    sigaction(SIGPIPE, &sact, NULL);
    sigaction(SIGTERM, &sact, NULL);
    
    /* run thhe main program loop */
    mux_loop(pf);
    
    /*unreachable line ??*/
    cleanup_termios(0);
}
