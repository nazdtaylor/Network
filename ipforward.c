// Naz Taylor
// CS 371
// Program Assignment 2

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

enum {MAXLINES = 1020};

typedef unsigned long UINT32;
typedef struct ipv4_hdr_f1 {
    UINT32 ver   : 4; //Ip version
    UINT32 iphl  : 4; //Internet Header Length
    UINT32 tos   : 8; //Type Of Service
    UINT32 len   : 16; //Total Length
} IPV4_HDR_F1;
typedef struct ipv4_hdr_f2 {
    UINT32 ident : 16; //Identification
    UINT32 flags : 4; //Flags
    UINT32 offset: 13; //Fragment Offset:
} IPV4_HDR_F2; 
typedef struct ipv4_hdr_f3 {
    UINT32 ttl   : 8; //Time To Live
    UINT32 proto : 8; //Protocol
    UINT32 cksum : 16; //checksum
} IPV4_HDR_F3;
typedef struct ipv4_header {
    IPV4_HDR_F1	f1; 
    IPV4_HDR_F2	f2;
    IPV4_HDR_F3 f3;
    UINT32 src; //Source Address
    UINT32 dest; //Destination Address
} IPV4_HEADER;

// Define arrays that will hold the components of teh forwarding table for comparison
char* netid[MAXLINES];
char* mask[MAXLINES];
char* hop[MAXLINES];

/* Function to start reading a file (fp) from a designated spot (offset) and
   store the contents in a specified buffer and a particular size. */

size_t fpread(void *buffer, size_t size, size_t mitems, size_t offset, FILE *fp)
{
     if (fseek(fp, offset, SEEK_SET) != 0)
         return 0;
     return fread(buffer, size, mitems, fp);
}

/* Funtion that take a forwarding table txt file as input and fills global
   arrays with the txt files contents to be accessed later in reference
   to the header values */

int parseForwardingTable(char* file){
    	// Initialization of variables including array that contains the forwarding tables contents
        int i = 0;
        char lines[MAXLINES][BUFSIZ];
        // Open file from command line that should be the forwarding table
        FILE *fp = fopen(file, "r");

        // If file cannot be opened
        if (fp == 0)
        {
                fprintf(stderr, "failed to open forwarding table file\n");
                exit(1);
        }
        // Split table up into individual lines
        while (i < MAXLINES && fgets(lines[i], sizeof(lines[0]), fp))
        {
                lines[i][strlen(lines[i])-1] = '\0';
                i = i + 1;
        }
        fclose(fp);
        // Loop though array lines that contains the forwarding table split into lines
        for(int x = 0; x < i; x++){
                // Split each line and put token into corresponding array for what the token is
                char *  p    = strtok (lines[x], " ");
                netid[x] = p;
                p = strtok(NULL, " ");
                mask[x] = p;
                p = strtok(NULL, " ");
                hop[x] = p;
                }
	return(i);
}

int main(int argc, char *argv[])
{
	// Initialize variable for later
	FILE *write_ptr; // output file pointer
	int i; // iteration that represents the size of the netid, mask, and hop arrays.
	unsigned char packetbuffer[20]; // Buffer that holds the header of the packets
	size_t offset=0; // offset value that determines where to start reading the file
	// Call function to parse forwarding table that is specified on the command line
	i = parseForwardingTable(argv[1]);
    	IPV4_HEADER  ipv4_hdr; //Declares a struct of type IPV4_HEADER
    	FILE *fd; //file name
    	int  nbytes; //Count number of bytes 
	// Make sure number of arguments on command line is correct
    	if (argc != 4) {
       		printf("Usage: %s <forwarding table> <filename> <output file>\n", argv[0]);
       		exit(-1);
    	}
	// Open file given on command line
    	fd = fopen(argv[2], "rb");

    	if (fd == 0) {
       		perror("open error");
       		exit(-1);
    	}
	// Count number of bytes in file
    	fread(packetbuffer, sizeof(packetbuffer),1,fd);


	// Open file to determine size
        int filesize = open(argv[2], O_RDONLY, 0664);
	// Count number of bytes in file
        nbytes = read(filesize, (void*)&ipv4_hdr, sizeof(IPV4_HEADER));
        // Error is read goes bad
        if (nbytes == -1) {
                perror("read error");
                exit(-1);
        }

	// Loop through the file until the end is reached
	while(offset < nbytes){
	// Decrease TTL by one
		ipv4_hdr.f3.ttl--;
		// Convert IP addresses to dotted format
		struct in_addr srcaddr;
		srcaddr.s_addr = htonl(ipv4_hdr.src);
		char *s = inet_ntoa(srcaddr);

		struct in_addr destaddr;
		destaddr.s_addr = htonl(ipv4_hdr.dest);
		char *d = inet_ntoa(destaddr);

		// The Following will print out desired components 
    		printf("Size             : %d \n", ipv4_hdr.f1.len);
   		// printf("TTL              : %d \n", ipv4_hdr.f3.ttl);
    		printf("Source IP Addr   : %s \n", s);
    		printf("Dest IP Addr     : %s \n", d);
		unsigned char buffer[ipv4_hdr.f1.len];

		// If TTL is 0 throw out the packet
		if(ipv4_hdr.f3.ttl == 0){
			printf("Packet thrown out because TTL is 0.");
		}
		// If TTL is not zero then determine the packets next hop address and write the packet to the output file specified on the command line
		else{
			for( int j = 0; j < i; j++)
				if (d == netid[i]){
					printf("Next Hop: %s\n", hop[i]);
				}
			// Creating output file and writing to it
			write_ptr = fopen(argv[3], "wb");
			fwrite(buffer, sizeof(buffer), 1, write_ptr);
		}
		// Move offset then start reading file again from there
		offset = offset + ipv4_hdr.f1.len;
		fpread(packetbuffer, 20, 1, offset, fd);
		}
    	return(0);
}
