// Naz Taylor
// CS 371
// Program Assignment 1 

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

#define BUFSIZE 8096
#define ERROR 42
#define NOGOOD 43
#define LOG   44

// The following is a list of supported file type extensions. All file types are not here I just put the more common ones i could think of but this is obvioulsy easily changed to sdupport more if needed.
struct {
	char *ext;
	char *filetype;
} extensions [] = {
	{"gif", "image/gif" },  
	{"jpg", "image/jpeg"}, 
	{"jpeg","image/jpeg"},
	{"png", "image/png" },  
	{"zip", "image/zip" },  
	{"tar", "image/tar" },  
	{"htm", "text/html" },  
	{"html","text/html" },  
	{"php", "image/php" },  
	{"cgi", "text/cgi"  },  
	{"jsp", "image/jsp" },  
	{"js","text/js"     },
	{0,0} };

// This function creates a log that is modified based on what the request does. It also keeps track of all the messages that the user sees and displays the proper message based on the situation encountered.
void log(int type, char *s1, char *s2, int num)
{
	// Variable initialization.
	int fd ;
	char logbuffer[BUFSIZE*2];

	// This sets up a switch statement to print error messages for the two error handling cases: ERROR and NOGOOD. It also has a statement for the LOG case which is when a good request happens and something needs to be added to the log being kept.
	switch (type) {
	 //ERROR occurs generally when there is an issue with the actual server such as placing it on a bad port or if there is an issue connecting sockets. 
	case ERROR: (void)sprintf(logbuffer,"ERROR: %s:%s Errno=%d exiting pid=%d",s1, s2, errno,getpid()); break;
	// The NOGOOD case occurs when there is a bad request such as asking for a file that does not exist or specifying a directory that doesnt exist.
	case NOGOOD: 
		(void)sprintf(logbuffer, "<HTML><BODY><H1>Server Issue: %s %s</H1></BODY></HTML>\r\n", s1, s2);
		(void)write(num,logbuffer,strlen(logbuffer));
		(void)sprintf(logbuffer,"Issue: %s:%s",s1, s2); 
		break;
	// This is the LOG case which is generally a good one and prints information about the specified request if needed.
	case LOG: (void)sprintf(logbuffer," INFO: %s:%s:%d",s1, s2,num); break;
	}	
	
	// This opens the log to be modifed if need be.
	if((fd = open("server.log", O_CREAT| O_WRONLY | O_APPEND,0644)) >= 0) {
		(void)write(fd,logbuffer,strlen(logbuffer)); 
		(void)write(fd,"\n",1);      
		(void)close(fd);
	}
	// If one of the two error cases occurs then quit whats going on.
	if(type == ERROR || type == NOGOOD) exit(3);
}

// This funstion handles the web interactions including reading the browser request and delivery what is requested if it exists.
void web(int fd, int hit)
{
	// vVariable initialization
	int j, file_fd, buflen, len;
	long i, ret;
	char * fstr;
	static char buffer[BUFSIZE+1];

	// This attempts to read the request and throws an error if it cannot.
	ret = read(fd,buffer,BUFSIZE); 
	if(ret == 0 || ret == -1) {
		log(NOGOOD,"failed to read browser request","",fd);
		(void)sprintf(buffer,"HTTP/1.0 404 Not Found\r\nContent-Type: %s\r\n\r\n", fstr);

	}
	// This moves things around in the buffer so that when something is added to it everything is in the right order.
	if(ret > 0 && ret < BUFSIZE)	
		buffer[ret]=0;	
	else buffer[0]=0;

	// This loops through the buffer and changes its contents if neccessary.
	for(i=0;i<ret;i++)	
		if(buffer[i] == '\r' || buffer[i] == '\n')
			buffer[i]='*';
	log(LOG,"request",buffer,hit);

	// This ensures only GET requests are attempted to be executed.
	if( strncmp(buffer,"GET ",4) && strncmp(buffer,"get ",4) )
		log(NOGOOD,"Only GET operation supported",buffer,fd);

	for(i=4;i<BUFSIZE;i++) { 
		if(buffer[i] == ' ') { 
			buffer[i] = 0;
			break;
		}
	}

	// This loops through the buffer and throws an error if an unsupported directry is detected based on characters in a cretain position and order in the bufffer.
	for(j=0;j<i-1;j++) 	
		if(buffer[j] == '.' && buffer[j+1] == '.')
			log(NOGOOD,"Parent directory specified is not one of the path names supported",buffer,fd);

	// This defaults to the page index.html if nothing else is specified in the request.
	if( !strncmp(&buffer[0],"GET /\0",6) || !strncmp(&buffer[0],"get /\0",6) ) 
		(void)strcpy(buffer,"GET /index.html");

	buflen=strlen(buffer);
	fstr = (char *)0;
	// This loops through the types of extensions and checks with the buffer to make sure the requested file is supported.
	for(i=0;extensions[i].ext != 0;i++) {
		len = strlen(extensions[i].ext);
		if( !strncmp(&buffer[buflen-len], extensions[i].ext, len)) {
			fstr =extensions[i].filetype;
			break;
		}
	}
	// Error thrown if requested file type is not supported.
	if(fstr == 0) log(NOGOOD,"File extension type not supported.",buffer,fd);

	// Error thrown if the file requested could not be opened.
	if(( file_fd = open(&buffer[5],O_RDONLY)) == -1) 
		log(NOGOOD, "Failed to open file.",&buffer[5],fd);
		
	// Buffer contents added to log if they are proper.
	log(LOG,"SEND",&buffer[5],hit);

	// Header response if everything checks out.
	(void)sprintf(buffer,"HTTP/1.0 200 OK\r\nContent-Type: %s\r\n\r\n", fstr);
	// If everything is in order then the contents are written.
	(void)write(fd,buffer,strlen(buffer));

	while (	(ret = read(file_fd, buffer, BUFSIZE)) > 0 ) {
		(void)write(fd,buffer,ret);
	}
}

// Main function for tying everything together. Mostly does socket work and forking.
int main(int argc, char **argv)
{
	// Vsaiable initialization
	int i, port, pid, listenfd, socketfd, hit;
	size_t length;
	static struct sockaddr_in cli_addr; 
	static struct sockaddr_in serv_addr;

	// Check for command line arguments to make sure there is the correct amount. Prints clarification of usage if command is bot formatted correctly.
	if( argc < 3  || argc > 3 || !strcmp(argv[1], "-?") ) {
		(void)printf("usage: server [port] [server directory]"
	"\tExample: server 8080 ./\n\n"
	// Lists supported file extensions if neccessary
	"\tOnly Supports:");
		for(i=0;extensions[i].ext != 0;i++)
			(void)printf(" %s",extensions[i].ext);
	// Lists unsuported directories if needed.
		(void)printf("\n\tNot Supported: directories / /etc /bin /lib /tmp /usr /dev /sbin \n"
		    );
		exit(0);
	}
	// Checks to see if specified directory is unsupported.
	if( !strncmp(argv[2],"/"   ,2 ) || !strncmp(argv[2],"/etc", 5 ) ||
	    !strncmp(argv[2],"/bin",5 ) || !strncmp(argv[2],"/lib", 5 ) ||
	    !strncmp(argv[2],"/tmp",5 ) || !strncmp(argv[2],"/usr", 5 ) ||
	    !strncmp(argv[2],"/dev",5 ) || !strncmp(argv[2],"/sbin",6) ){
		(void)printf("ERROR: Bad top directory %s, see server -?\n",argv[2]);
		exit(3);
	}

	// Forking procedure if everything to this point is correct.
	if(fork() != 0)
		return 0; 
	(void)signal(SIGCLD, SIG_IGN); 
	(void)signal(SIGHUP, SIG_IGN); 
	for(i=0;i<32;i++)
		(void)close(i);	
	(void)setpgrp();	

	log(LOG,"http server starting",argv[1],getpid());
	// Error thrown for incorrect number of sockets.
	if((listenfd = socket(AF_INET, SOCK_STREAM,0)) <0)
		log(ERROR, "system call","socket",0);
	port = atoi(argv[1]);
	// Error thrown if port is out of range
	if(port < 2000 || port >60000)
		log(ERROR,"Invalid port number try [2001,60000]",argv[1],0);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(port);
	// Error thrown if there is an issue binding.
	if(bind(listenfd, (struct sockaddr *)&serv_addr,sizeof(serv_addr)) <0)
		log(ERROR,"system call","bind",0);
	// Error thrown if there is an issue listening
	if( listen(listenfd,64) <0)
		log(ERROR,"system call","listen",0);
	// Socket acceptance for specified number of sockets.
	for(hit=1; ;hit++) {
		length = sizeof(cli_addr);
		if((socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length)) < 0)
			log(ERROR,"system call","accept",0);
	// Error thrown if bad fork is attempted
		if((pid = fork()) < 0) {
			log(ERROR,"system call","fork",0);
		}
	// Closing of remaining sockets if needed
		else {
			if(pid == 0) {
				(void)close(listenfd);
				web(socketfd,hit);
			} else {
				(void)close(socketfd);
			}
		}
	}
}
