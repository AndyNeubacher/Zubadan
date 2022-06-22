#include <errno.h>
#include <fcntl.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

// compile: gcc -Wall dump_uart.c -o dump_uart


#define CHAR_TIMEOUT_MS		40
#define NAME_SERIAL_PORT	"/dev/ttyS1"
#define NAME_LOGFILE		"/home/python/log_c.txt"
#define NAME_LOGFILE_RAW	"/home/python/log_c_raw.txt"






int32_t time_start = 0;
int32_t time_last_char = 0;
volatile int execute;

FILE* fd_logfile;
FILE* fd_lograw;
int   fd_serial;

struct sigaction old_action;




void sigint_handler(int sig_no)
{
	printf("\n\nCTRL-C pressed ... closing all logfiles\n");
	fclose(fd_logfile);
	fclose(fd_lograw);
	close(fd_serial);

	sigaction(SIGINT, &old_action, NULL);
	kill(0, SIGINT);
}



int32_t get_time()
{
	struct timespec tms;

	if (clock_gettime(CLOCK_REALTIME,&tms))
		return -1;

	int32_t micros = tms.tv_sec * 1000000;	/* seconds, multiplied with 1 million */
	micros += tms.tv_nsec/1000;		/* Add full microseconds */
	if (tms.tv_nsec % 1000 >= 500)		/* round up if necessary */
		++micros;

	return (micros/1000);
}



int set_interface_attribs(int fd, int speed)
{
	struct termios tty;

	if (tcgetattr(fd, &tty) < 0)
	{
		printf("Error from tcgetattr: %s\n", strerror(errno));
		return -1;
	}

	cfsetospeed(&tty, (speed_t)speed);
	cfsetispeed(&tty, (speed_t)speed);

	tty.c_cflag |= (CLOCAL | CREAD);	/* ignore modem controls */
	tty.c_cflag &= ~CSIZE;
	tty.c_cflag |= CS8;		 	/* 8-bit characters */
	tty.c_cflag &= ~PARENB;	 		/* no parity bit */
	tty.c_cflag &= ~CSTOPB;	 		/* only need 1 stop bit */
	tty.c_cflag &= ~CRTSCTS;		/* no hardware flowcontrol */

	/* setup for non-canonical mode */
	tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
	tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	tty.c_oflag &= ~OPOST;

	/* fetch bytes as they become available */
	tty.c_cc[VMIN] = 1;
	tty.c_cc[VTIME] = 1;

	if (tcsetattr(fd, TCSANOW, &tty) != 0)
	{
		printf("Error from tcsetattr: %s\n", strerror(errno));
		return -1;
	}
	return 0;
}



void set_mincount(int fd, int mcount)
{
	struct termios tty;

	if (tcgetattr(fd, &tty) < 0)
	{
		printf("Error tcgetattr: %s\n", strerror(errno));
		return;
	}

	tty.c_cc[VMIN] = mcount ? 1 : 0;
	tty.c_cc[VTIME] = 5;		/* half second timer */

	if (tcsetattr(fd, TCSANOW, &tty) < 0)
		printf("Error tcsetattr: %s\n", strerror(errno));
}



void logger(char* str, unsigned char val, FILE* log, FILE* raw)
{
	int32_t time_now, time_delta=0;
	
	
	if(time_start==0)
	{
		time_start = get_time();
		time_last_char = time_start;
		sprintf(str, "0ms: %02x", val);
	}
	else
	{
		time_now = get_time();
		time_delta = time_now - time_start;
		if( (time_now - time_last_char) < CHAR_TIMEOUT_MS)
			sprintf(str, " %02x", val);
		else
			sprintf(str, "\n%06lums: %02x", time_delta, val);

		time_last_char = time_now;
	}
	
	printf("%s", str);				// log to console
	fputs(str, log);				// log to file
	fprintf(raw, "%06lu: %02x\n", time_delta, val);	// log raw data to file
}



int main()
{
	char str[200];
	char *portname = NAME_SERIAL_PORT;
	struct sigaction action;




	memset(&action, 0, sizeof(action));
	action.sa_handler = &sigint_handler;
	sigaction(SIGINT, &action, &old_action);
	

	/* open logfile CREATE+APPEND */
	fd_logfile = fopen(NAME_LOGFILE, "a+");
	fd_lograw  = fopen(NAME_LOGFILE_RAW, "a+");
	if( (fd_logfile < 0) || (fd_lograw < 0) )
	{
		printf("Error opening logfiles: %s\n", strerror(errno));
		return -1;
	}

	/* open serial port */
	fd_serial = open(portname, O_RDWR | O_NOCTTY | O_SYNC);
	if (fd_serial < 0)
	{
		printf("Error opening %s: %s\n", portname, strerror(errno));
		return -1;
	}
	
	
	set_interface_attribs(fd_serial, B2400);	// baudrate 2400, 8 bits, no parity, 1 stop bit
	//set_mincount(fd_serial, 0);			// set to pure timed read


	printf("Start reading!\n");

	do
	{
		unsigned char buf[2];
		int rdlen;

		rdlen = read(fd_serial, buf, 1);
		if (rdlen > 0)
			logger(&str[0], buf[0], fd_logfile, fd_lograw);
		else if (rdlen < 0)
			printf("Error from read: %d: %s\n", fd_serial, strerror(errno));
			
	} while (1);
}


