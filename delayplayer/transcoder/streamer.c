#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <dirent.h> 
#include <time.h>
#include <stdarg.h>
//#include  <sys/ioctl.h>

char *server_name = "Delayplayer v0.2";

#define BUFSIZE 8096
#define ERROR 42
#define SORRY 43
#define LOG   44

// chunk size to send to clients @ 48kbps or whatever
#define BITRATE		56000
#define SAMPLERATE	22050
#define CHANNELS	2

#define FPS			38

#define FRAMESIZE			( (114 * BITRATE) / SAMPLERATE)
#define BYTES_BER_SECOND 	(FRAMESIZE * 38)

//#define TX_BUFSIZE	7000
#define BURSTSIZE	200000


void sock_printf(int fd, char *fmt, ...)
{
	char buffer[1024];
	
    va_list args;
    va_start(args,fmt);
    vsprintf(buffer,fmt,args);
    va_end(args);
    
	(void)write(fd,buffer,strlen(buffer));
}

int get_value(char *querystring, char *key, char *buffer, int length)
{
	char *ptr = strstr(querystring,key);
	
	if(ptr!=NULL)
	{
		ptr+=strlen(key);
		
		if(*ptr=='=')
		{
			ptr++;
			int c=0;
			
			// copy the value to the caller's buffer
			while(*ptr!=0 && *ptr!='&' && c<(length-1))
			{
				buffer[c]=*ptr;
				c++;
				ptr++;
			}
			buffer[c]=0;
			return 0;
		}
	}
	return -1;
}

void log(char *message)
{
        FILE *fd ;
        char logbuffer[BUFSIZE*2];
		time_t ltime;
		struct tm *Tm;
 
		ltime=time(NULL);
		Tm=localtime(&ltime);

        (void)sprintf(logbuffer,"%02d/%02d/%04d %02d:%02d:%02d PID %d %s",
			Tm->tm_mday,
            Tm->tm_mon+1,
            Tm->tm_year+1900,
            Tm->tm_hour,
            Tm->tm_min,
            Tm->tm_sec,
            getpid(),
            message);

		if((fd = fopen("/tmp/streamer.log", "a")) != NULL)
		{
			fprintf(fd,"%s\n",logbuffer);
			fclose(fd);
		}
}

void http_response(int fd, char *code)
{
	char header[512];
	sprintf(header,"HTTP/1.1 %s\r\n"
					"Content-Type:	text/html\r\n\r\n"
					"%s\r\n",code,code);
	(void)write(fd,header,strlen(header));
}

// %04d-%02d-%02d-%02d-%02d-%02d", $year+1900,$mon+1,$mday,$hour,$min,$sec
// 2012-03-16-13-06-56
time_t date_to_epoch(char *txtdate)
{
	struct tm date;

	if(strlen(txtdate)<19)
		return 0;

	// init struct
	memset(&date,0,sizeof(struct tm));

	date.tm_year	= atoi(&txtdate[0])-1900;
	date.tm_mon		= atoi(&txtdate[5])-1;
	date.tm_mday	= atoi(&txtdate[8]);

	date.tm_hour	= atoi(&txtdate[11]);
	date.tm_min		= atoi(&txtdate[14]);
	date.tm_sec		= atoi(&txtdate[17]);

	time_t rawtime;
	rawtime = mktime(&date);

	return(rawtime);
}

// return the filename of the file closest to the target time
int get_next_file(char *dir_path, time_t target, char *filename)
{
	struct dirent * dp; 
	time_t epoch,min;

	min = 9999999;

	// default null string
	*filename=0;

	fprintf (stderr, "get_next_file %s\t%d\n", dir_path,target);

	DIR * dir = opendir (dir_path); 
	
	if(dir==NULL)
	{
		fprintf (stderr, "dir is null\n");
		return 0;
	}

	while ((dp = readdir (dir))!=NULL)
	{
//		fprintf (stderr, "name: %s\n", dp->d_name,epoch);
		epoch = date_to_epoch(dp->d_name);

		// find which file we currently should be in
		int diff = target-epoch;
		
		if(diff>0 && diff<min)
		{
			min=diff;
			strcpy(filename,dp->d_name);
		}
	} 
	
	fprintf (stderr, "get_next_file finished - new file: %s\n",filename);

	closedir(dir);

	if(min == 9999999)
		return -1;

	return min;
}

/* this is a child web server process, so we can exit on errors */
void stream(int fd, char *fullpath)
{
	int delay;
	char header[1024];
	long ret;
	char value[128];

	static char buffer[BUFSIZE+1]; /* static so zero filled */

	ret =read(fd,buffer,BUFSIZE);   /* read Web request in one go */
	if(ret == 0 || ret == -1)
	{     /* read failure stop now */
		printf("Request read failed\n");

log("Stopping, couldn't read client headers");

		exit(0);
	}

	// terminate
	buffer[ret]=0;

	// get query 
	char *ptr = strstr(buffer, "GET ");

	if(ptr==NULL)
	{
		http_response(fd,"404 Not Found");
	}
	else
	{
		ptr+=4;
		
		char *query = ptr;		
		char *ptr = strchr(query,' ');

		if(*ptr!=NULL)
			*ptr=0;

		if(get_value(query,"delay",value,128)<0)
		{
			http_response(fd,"400 Bad Request");
log("400 (delay)");
			exit(-1);
		}
		delay = atoi(value);

		if(get_value(query,"path",value,128)<0)
		{
			http_response(fd,"400 Bad Request");
log("400 (path)");
			exit(-1);
		}

		sprintf(header,"HTTP/1.0 200 OK\r\n"
						"Content-Type: %s\r\n"
						"Accept-Ranges: none\r\n"
						"icy-br:48\r\n"
						"icy-description:%s\r\n"
						"icy-genre:%s\r\n"
						"icy-name:%s\r\n"
						"icy-info:0\r\n"
						"icy-pub:1\r\n"
						"icy-reset:1\r\n"
						"Server: %s\r\n\r\n",
						"text/plain","Radio1", "BBC Radio", "BBC Radio 1",server_name);

		(void)write(fd,header,strlen(header));

		// write data
/*		
		// some debug stuff...
		(void)write(fd,buffer,strlen(buffer));

		sprintf(header,"\r\nDelay = %d\r\nPath = %s\r\n",delay, value);
		(void)write(fd,header,strlen(header));
		// end debug stuff
*/
		char path[512];
		
		strcpy(path,fullpath);
		strcat(path,value);

		fprintf(stderr,"Starting %s\n",path);
		start_streaming(fd,path,delay);
		fprintf(stderr,"Finished\n");
		close(fd);
		fprintf(stderr,"Disconnected\n");
	}
	
	#ifdef LINUX

        sleep(1);       /* to allow socket to drain */

	#endif

log("Server thread finished");

        exit(1);
}

time_t get_delay_epoch(int delay)
{
	return time(NULL)-(delay*60);
}

int set_timeout(int socketfd)
{
	struct timeval timeout;      
	timeout.tv_sec = 10;
	timeout.tv_usec = 0;

//	ioctl(socketfd, FIONBIO, &opt);

	if (setsockopt (socketfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout,sizeof(timeout)) < 0)
	{
		fprintf(stderr,"setsockopt failed SO_RCVTIMEO\n");
		return -1;
	}
	
	if (setsockopt (socketfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout,sizeof(timeout)) < 0)
	{
		fprintf(stderr,"setsockopt failed SO_SNDTIMEO\n");
		return -1;
	}
	
	return 0;
}


int setNonblocking(int fd)
{
    int flags;

    /* If they have O_NONBLOCK, use the Posix way to do it */
#if defined(O_NONBLOCK)
    /* Fixme: O_NONBLOCK is defined but broken on SunOS 4.1.x and AIX 3.2.5. */
    if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
        flags = 0;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
    /* Otherwise, use the old way of doing it */
    flags = 1;
    return ioctl(fd, FIOBIO, &flags);
#endif
}     

int start_streaming(int fd, char *path, int delay)
{
	char firstfail=0;
	
	int ret;
	char temp[512];
	char filename[512];
	char lastfilename[512];
	char buffer[BYTES_BER_SECOND+1];
	FILE *inputfile;
	long total_bytes;

sprintf(buffer, "Starting server %s %d seconds",path, delay);
log(buffer);

fprintf(stderr, "start_streaming\n");

	do
	{
		int offset = get_next_file(path,get_delay_epoch(delay),temp);
		
		if(offset<0)
		{
			fprintf(stderr,"Couldn't find an initial file\n");
		log("Couldn't find an initial file");
			return(-1);
		}

		fprintf(stderr, "get_next_file returned %s\n",temp);

		// build full filename
		strcpy(filename,path);
		strcat(filename,"/");
		strcat(filename,temp);
		
		strcpy(lastfilename,filename);

		fprintf(stderr, "Opening %s\n",filename);

		total_bytes=0;
		inputfile=fopen(filename,"rb");

		if(inputfile==NULL)
		{
			fprintf(stderr,"Couldn't open %s\n",filename);
		log("Couldn't open initial file");
			return(-1);
		}

		fprintf(stderr,"Offset %d seconds\n",offset);

		//ffwd to offset in file
		int fileoffset = BYTES_BER_SECOND * offset;
		fprintf(stderr,"Stream rate %d bytes/second\n",BYTES_BER_SECOND);
		fprintf(stderr,"Seeking forward %d bytes\n",fileoffset);

		// find next MP3 frame header
		  // finding first frame for MPEG 1 LAYER III
		int ch;
		do
		{
			ch=fgetc(inputfile);
			if(ch==0xff)
			{
				ch=fgetc(inputfile);
				if((ch&0xE0)==0xE0)
				{
					fprintf(stderr,"Found MP3 header at position %d\n",ftell(inputfile));
					// rewind to start of frame
					fseek(inputfile,-2,SEEK_CUR);
					break;
				}
			}
		}while(!feof(inputfile));
	}
    while(feof(inputfile));
    
	fprintf(stderr,"Streaming started\n");

//return(0);

	fd_set write_flags;
	struct timeval timeout;      
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;
//	fd_set rsd = read_sd;

	int sel;

set_timeout(fd);
setNonblocking(fd);
	// stream until client socket is disconnected
	// or an error occurs
	while(1)
	{
	fprintf(stderr,"a\n");

	fprintf(stderr,"b\n");

	fprintf(stderr,"c\n");
		/* send stream in blocks */
		while( inputfile!=NULL && (ret = fread(buffer, sizeof(char),BYTES_BER_SECOND,inputfile)) > 0 )
		{
			
			fprintf(stderr,"d\n");
			
	/*
			if(ret<TX_BUFSIZE)
			{
				// read some more data to make the block size up
				get_next_file(path,get_delay_epoch(delay-1),filename);
				// build full filename
				strcpy(filename,path);
				strcat(filename,temp);

				strcpy(lastfilename,filename);

	get_next_file(path,time(NULL),temp);


	strcpy(lastfilename,filename);

			}
	*/				

			int retval = recv(fd, temp, 512, MSG_PEEK | MSG_DONTWAIT);
			fprintf(stderr,"recv returned %d\n",retval);
			
			if(retval==0)
			{
				log("Recv returned 0, waiting and retrying...");
				sleep(1);
				// have another go
				retval = recv(fd, temp, 512, MSG_PEEK | MSG_DONTWAIT);

				if(retval==0)
				{
					fprintf(stderr,"Client disconnected (recv returned %d)\n",retval);
					log("Client disconnected (recv returned 0 twice)");
					return(0);
				}
				else
				{
					if(retval>0)
					{
						retval = read(fd, temp, 511);
						temp[retval]=0;
						fprintf(stderr,"Read %d bytes from socket: %s\n",retval,temp);
					}
					log("Recv returned !0, continuing");
				}
			}
			else
			{
				if(retval>0)
				{
					retval = read(fd, temp, 511);
					temp[retval]=0;
					fprintf(stderr,"Read %d bytes from socket: %s\n",retval,temp);
				}
			}

	// wait forever for client connection
	fd_set rwf;
	struct timeval timeout;      
	timeout.tv_sec = 0;
	timeout.tv_usec = 1000;
	FD_ZERO(&rwf);
	FD_SET(fd, &rwf);
	int sel = select(fd, 0, &rwf, 0, &timeout);
	if(FD_ISSET(fd, &rwf))
		fprintf(stderr,"socket ready for writing\n");
	else	
		fprintf(stderr,"socket NOT ready for writing\n");
		
		// TODO: timeout
		
	///////////////////////////////////////////


			int bytes_sent;
			int offset=0;
			
			do
			{
				// send data to client
				fprintf(stderr,"Sending %d bytes...",ret);
				bytes_sent = send(fd, buffer+offset, ret, 0);
				total_bytes+=bytes_sent;
				fprintf(stderr,"done, sent %d bytes (total %d) %s\n",bytes_sent,total_bytes,filename);

				if(bytes_sent<=0)
				{
					// TODO: timeout
					sleep(1);
					log("Retrying, write returned <=0");
					fprintf(stderr,"Retrying, write returned %d\n",bytes_sent);
//					return(0);
				}
				else
				{
					// adjust offsets
					offset+=bytes_sent;
					ret-=bytes_sent;
				}

			}
			while(ret);

			// regulate the stream
			if(total_bytes>=BURSTSIZE)
			{
				sleep(1); /* to allow socket to drain */
			}
		}
		

		if(inputfile!=NULL)
			fclose(inputfile);
		
		// load up the next file
		inputfile=NULL;


		// check client still connected
		char temp[32];
		int retval = recv(fd, temp, 32, MSG_PEEK | MSG_DONTWAIT);
		fprintf(stderr,"recv returned %d\n",retval);
		
		if(retval==0)
		{
			fprintf(stderr,"Client disconnected (recv returned %d)\n",retval);
		log("Client disconnected (recv returned 0)");
			return(0);
		}


		if(get_next_file(path,get_delay_epoch(delay-1),temp)>=0)
		{
			// build full filename
			strcpy(filename,path);
			strcat(filename,"/");
			strcat(filename,temp);

			if(strcmp(filename,lastfilename)!=0)
			{
				strcpy(lastfilename,filename);

				inputfile=fopen(filename,"rb");

				if(inputfile==NULL)
				{
					fprintf(stderr,"Couldn't open %s\n",filename);
					log("Couldn't open next file");
					return(-1);
				}
				else
				{
					log("Opened new file");
					fprintf(stderr,"Opened new file: %s\n",filename);
				}
			}
			else
			{
					log("Waiting to start next file...");
				fprintf(stderr,"Waiting to start next file...\n");
				sleep(1);
			}
		}
		else
		{
					log("get_next_file failed");
			fprintf(stderr,"get_next_file failed\n");
			sleep(1);
		}			
	}

			log("Streaming finished");
	fprintf(stderr,"Streaming finished\n");

	return(0);
}

main(int argc, char **argv)
{
	int listenfd;
	int i, port, pid, socketfd, hit;
	size_t length;
	char *str;
	static struct sockaddr_in cli_addr; /* static = initialised to zeros */
	static struct sockaddr_in serv_addr; /* static = initialised to zeros */
/*
time_t t = date_to_epoch("2012-03-16-13-06-57.xyz");

printf("now  = %d\n",(int)time(NULL));
printf("time = %d\n",(int)t);
*/

	if( argc < 3  || argc > 3 || !strcmp(argv[1], "-?") )
	{
			(void)printf("hint: nweb Port-Number Top-Directory\n\n"
	"\tnweb is a small and very safe mini web server\n"
	"\tnweb only servers out file/web pages with extensions named below\n"
	"\t and only from the named directory or its sub-directories.\n"
	"\tThere is no fancy features = safe and secure.\n\n"
	"\tExample: nweb 8181 /home/nwebdir &\n\n"
	"\tOnly Supports:");

			(void)printf("\n\tNot Supported: URLs including \"..\", Java, Javascript, CGI\n"
	"\tNot Supported: directories / /etc /bin /lib /tmp /usr /dev  /sbin \n"
	"\tNo warranty given or implied\n\tNigel Griffiths nag@uk.ibm.com\n"
				);

		log("Can't start server, invalid arguments");

		exit(0);
	}

	// Become daemon + unstopable and no zombies children (= no wait())
	if(fork() != 0)
			return 0; // parent returns OK to shell

	(void)signal(SIGCLD, SIG_IGN); // ignore child death
	(void)signal(SIGHUP, SIG_IGN); // ignore terminal hangups

	for(i=0;i<32;i++)
			(void)close(i);      // close open files

	(void)setpgrp();             // break away from process group

fprintf(stderr, "starting\n");


		fprintf(stderr, "nweb starting!\n");

	/* setup the network socket */
//	if((listenfd = socket(AF_INET, SOCK_STREAM|SOCK_NONBLOCK,0)) <0)
	if((listenfd = socket(AF_INET, SOCK_STREAM,0)) <0)
	{
		fprintf(stderr, "socket failed\n");
		log("Can't start server, socket failed");
		exit(-1);
	}


	/* Allow socket descriptor to be reusable */
	int yes=1;
	if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(int)) == -1)
	{
		fprintf(stderr, "setsockopt failed\n");
		log("Can't start server, setsockopt failed");
		close(listenfd);
		exit(1);
	}

	port = atoi(argv[1]);

	if(port < 0 || port >60000)
	{
		fprintf(stderr, "Invalid port number (try 1->60000)\n");
		log("Can't start server, invalid port number");
		exit(-1);
	}
	
	memset(&serv_addr, 0x00, sizeof(struct sockaddr_in));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(port);

	if(bind(listenfd, (struct sockaddr *)&serv_addr,sizeof(serv_addr)) <0)
	{
		fprintf(stderr, "bind failed\n");
		log("Can't start server, bind failed");
		close(listenfd);
		exit(-1);
	}

	if( listen(listenfd,64) <0)
	{
		fprintf(stderr, "listen failed\n");
		log("Can't start server, listen failed");
		close(listenfd);
		exit(-1);
	}
	for(hit=1; ;hit++)
	{
		
	// wait forever for client connection
/*
	fd_set rwf;
	struct timeval timeout;      
	timeout.tv_sec = 3600;
	timeout.tv_usec = 0;
	FD_ZERO(&rwf);
	FD_SET(listenfd, &rwf);
	int sel = select(listenfd+1, &rwf, 0, 0, &timeout);
	if(FD_ISSET(listenfd, &rwf))
		fprintf(stderr,"socket ready for reading\n");
	else	
		fprintf(stderr,"socket NOT ready for reading\n");
*/
	///////////////////////////////////////////

	   length = sizeof(cli_addr);
		if((socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length)) < 0)
		{
			fprintf(stderr, "accept failed\n");
			log("Can't start server, accept failed");
			exit(-1);
		}


//int flags = fcntl(listenfd, F_GETFL, 0);
//fcntl(listenfd, F_SETFL, flags | O_NONBLOCK);
int flags = fcntl(socketfd, F_GETFL, 0);
fcntl(socketfd, F_SETFL, flags | O_NONBLOCK);

//		if(set_timeout(socketfd)>=0)
			if((pid = fork()) < 0)
			{
					fprintf(stderr,"fork returned <0\n");
			}
			else
			{
				if(pid == 0)
				{  /* child */
					(void)close(listenfd);

					stream(socketfd,argv[2]); /* never returns */
				}
				else
				{        /* parent */
					(void)close(socketfd);
				}
			}
	}

log("Stopping server");

}
