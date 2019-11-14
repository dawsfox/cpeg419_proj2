/* udpclient.c */ 
/* Programmed by Matt Stack and Dawson Fox */
/* Nov. 13, 2019 */     

#include <stdio.h>          /* for standard I/O functions */
#include <stdlib.h>         /* for exit */
#include <string.h>         /* for memset, memcpy, and strlen */
#include <netdb.h>          /* for struct hostent and gethostbyname */
#include <sys/socket.h>     /* for socket, connect, send, and recv */
#include <netinet/in.h>     /* for sockaddr_in */
#include <unistd.h>         /* for close */
#include <stdbool.h>	    /* bool library  */

#define GRAM_SIZE 84 //size of max gram size plus 4 byte header

/* NOTE: I know that the help document suggests wrapping the entire segment
 * in a structure for transmission. I decided to play around with union and
 * converting shorts to chars and vice versa in order to learn more about
 * the language and be able to only submit a single char array instead of
 * an entire struct.
 */

/* union used to convert characters to short and back for easy transmission */
union {
   char chars[2];
   short n;
} char_conv;


/* function to designate whether or not ACK should
 * be decided as dropped based on user-give loss ratio
 */
bool simulate_ack_loss(float ack_loss_ratio) {
	float rando = (float) (((float)rand()) / ((float)RAND_MAX));
	return (rando < ack_loss_ratio);
}


/* uses the globally declared union above
 * to convert a short to two chars, returns
 * first byte
 */
char convert_to_char1(short n) {
   char_conv.n = n;
   return char_conv.chars[0];
}

/* uses the globablly declared union above
 * to convert a short to two chars, returns
 * second byte
 */
char convert_to_char2(short n) {
   char_conv.n = n;
   return char_conv.chars[1];
}

/* uses the globally declared union above
 * to convert two chars to a short, returns
 * the value of the short
 */
short convert_to_short(char first, char second) {
   char_conv.chars[0] = first;
   char_conv.chars[1] = second;
   return char_conv.n;
}

int main(void) {

   int sock_client;  /* Socket used by client */

   struct sockaddr_in server_addr;  /* Internet address structure that
                                        stores server address */
   struct hostent * server_hp;      /* Structure to store server's IP
                                        address */
   char server_hostname[GRAM_SIZE] = "cisc450.cis.udel.edu"; /* Server's hostname */
   unsigned short server_port;  /* Port number used by server (remote port) */

   unsigned int msg_len;  /* length of message */                      
   int bytes_sent, bytes_recd; /* number of bytes sent or received */

   float ack_loss_ratio = 0; /* user-given number to decide percentage of dropped ACKs */

   /* open a socket */

   if ((sock_client = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
      perror("Client: can't open stream socket");
      exit(1);
   }

   /* initialize server address information */

   if ((server_hp = gethostbyname(server_hostname)) == NULL) {
      perror("Client: invalid server hostname");
      close(sock_client);
      exit(1);
   }

   /* prompt for port server is operating on */
   printf("Enter port number for server: ");
   scanf("%hu", &server_port);

   /* Clear server address structure and initialize with server address */
   memset(&server_addr, 0, sizeof(server_addr));
   server_addr.sin_family = AF_INET;
   memcpy((char *)&server_addr.sin_addr, server_hp->h_addr,
                                    server_hp->h_length);
   server_addr.sin_port = htons(server_port);

   /* user interface */
   char file_request[GRAM_SIZE]; //char array holds header in char format and payload of filename
   printf("Please input a filename: \n");
   scanf("%s", &file_request[4]); //file name goes after 4 bytes of header content
   int name_len = strlen(&file_request[4]) + 1;
   file_request[0] = convert_to_char1(name_len); //sets byte 0 equal to first byte of 2-byte payload length
   file_request[1] = convert_to_char2(name_len); //sets byte 1 equal to second byte of payload length
   file_request[2] = 0; //upper 8 bits of sequence number will always be 0s since we are using ABP
   file_request[3] = 0; //sequence is 0 on filename packet
   msg_len = name_len + 4; // length of filename plus 4 header bytes
   printf("Please enter an ACK loss ratio between 0 and 1:\n");
   scanf("%f", &ack_loss_ratio);

   /* send filename */
   
   bytes_sent = sendto(sock_client, file_request, msg_len, 0, (struct sockaddr *) &server_addr, sizeof(server_addr));

   /* get response from server */

   int data_total = 0;  //used to track total number of bytes transferred
   bool end_of_file = false;
   FILE *output_file = fopen("out.txt", "w");
   char seg_recv[GRAM_SIZE];
   short sequence; //2 bytes for sequence num
   short expected_sequence = 0; //2 bytes for expected sequence num to be received
   short line_size = 0; //used to receive payload length of incoming packets
   int count = 0; //counts number of successfully received data packets
   int dup_count = 0; //counts number of duplicate packets received
   int og_count = 0; //counts number of original non-duplicate packets received
   int ack_count = 0; //counts number of ACKs generated (both sent and lost)
   int sent_count = 0; //counts number of successfully sent ACKs
   int loss_count = 0; //counts number of ACKs designated to be dropped/lost

   while (end_of_file != true){	 //keep reading file until EOF character

	/* have to receive whole array to check header, then only read that much data */
   	recvfrom(sock_client, seg_recv, GRAM_SIZE, 0, (struct sockaddr *) NULL, NULL);
	if (seg_recv[0] == 0 && seg_recv[1] == 0){ // EOF packet received (payload length is 0), no ACK needs to be sent
		sequence = (unsigned short) seg_recv[2]; //lower 8 of sequence num received
		line_size = convert_to_short(seg_recv[0], seg_recv[1]); //converts 2 char bytes for payload len to one 2-byte short
		printf("\nEnd of Transmission Packet with sequence number %d received\n\n", sequence);
		end_of_file = true; //breaks loop
		fclose(output_file);

		printf("Total number of data packets successfully received: %d\n", count);
		printf("Number of duplicate data packets received: %d\n", dup_count);
		printf("Number of original data packets received (not duplicates): %d\n\n", og_count);

		printf("Total number of data bytes delivered to user: %d\n\n", data_total);

		printf("Number of ACKs transmitted successfully: %d\n", sent_count);
		printf("Number of ACKS lost: %d\n", loss_count);
		printf("Total number of ACKs generated: %d\n\n", ack_count);
	} //if EOF received
	else { //if not EOF
		sequence = (unsigned short) seg_recv[2]; //lower 8 of sequence num received
		line_size = convert_to_short(seg_recv[0], seg_recv[1]); //convert two chars of payload length into one short
		if (sequence != expected_sequence) { //if wrong sequence received, packet is duplicate
			count++;
			dup_count++;
			printf("\tDuplicate packet %d received with %d data bytes \n", sequence, line_size); 
			ack_count++;
			if (simulate_ack_loss(ack_loss_ratio)) { //if ACK is designated to be dropped/lost
				printf("\tACK %d lost\n", sequence);
				loss_count++;
			}
			else { //if ACK is designated to be sent successfully
				/* send correct ACK (same sequence num as expected) */
				bytes_sent = sendto(sock_client, &sequence, 2, 0, (struct sockaddr *) &server_addr, sizeof(server_addr));
				sent_count++;
				//do not deliver data, duplicate
				printf("\tACK %d sent\n", sequence);
			}
		} //if duplicate
		else{ //if correct sequence number, new data packet
			printf("Packet %d received with %d data bytes \n", sequence, line_size);
			data_total += line_size; //increment delivered data bytes count
			count++; //increment packet count
			og_count++;
			fputs(&seg_recv[4], output_file); //add to file line by line, rather than char by char like server
			printf("Packet %d delivered to user\n", sequence);
			printf("\tACK %d generated for transmission\n", sequence);
			ack_count++;
			if (simulate_ack_loss(ack_loss_ratio)) { //if ACK is designated to be dropped/lost
				loss_count++;
				printf("\tACK %d lost\n", sequence);
			}
			else { //if ACK designated to be successfully sent
				sent_count++;
				/* send correct ACK */
				bytes_sent = sendto(sock_client, &sequence, 2, 0, (struct sockaddr *) &server_addr, sizeof(server_addr));
				printf("\tACK %d successfully transmitted\n", sequence);
			}
			expected_sequence = 1 - expected_sequence; // flip expected sequence num after delivering data
		} //else (not duplicate)
		memset(seg_recv, 0, GRAM_SIZE); //clear char array for new receiving data
	} //else (not EOF)	
   } // while EOF not reached
  
   /* close the socket */
   close (sock_client);

} //main
