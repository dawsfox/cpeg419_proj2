/* udpserver.c */
/* Programmed by Matt Stack and Dawson Fox */
/* Nov. 13, 2019 */    

#include <ctype.h>          /* for toupper */
#include <stdio.h>          /* for standard I/O functions */
#include <stdlib.h>         /* for exit */
#include <string.h>         /* for memset */
#include <sys/socket.h>     /* for socket, bind, listen, accept */
#include <netinet/in.h>     /* for sockaddr_in */
#include <unistd.h>         /* for close */
#include <stdbool.h>	    /* bool library */

#define GRAM_SIZE 84 //80 data bytes plus 4 byte header

/* SERV_UDP_PORT is the port number on which the server listens for
   incoming requests from clients. You should change this to a different
   number to prevent conflicts with others in the class. */

#define SERV_UDP_PORT 65001

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

/* function used to decide if outgoing packet should be
 * simulated as lost (dropped) or if it should be
 * successfully transmitted
 */
bool simulate_loss(float packet_loss_ratio) {
   float rando = (float) (((float)rand()) / ((float)RAND_MAX));
   return (rando < packet_loss_ratio);
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
	
   srand(time(0));  /* Seed random function so that nums actually change */
   int sock_server;  /* Socket on which server listens to clients */

   struct sockaddr_in server_addr;  /* Internet address structure that
                                        stores server address */
   unsigned int server_addr_len;  /* Length of server address structure */
   unsigned short server_port;  /* Port number used by server (local port) */

   struct sockaddr_in client_addr;  /* Internet address structure that
                                        stores client address */
   unsigned int client_addr_len;  /* Length of client address structure */

   short msg_len;  /* length of message */
   int bytes_sent, bytes_recd; /* number of bytes sent or received */
   float packet_loss_ratio = 0; /* used for simulated packet loss */
   int timeout_input = 0; /* int from 1 to 10 to designate timeout length */
   struct timeval timeout;
   unsigned int i;  /* temporary loop variable */

   /* open a socket */

   if ((sock_server = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
      perror("Server: can't open datagram socket");
      exit(1);                                                
   }

   /* initialize server address information */
    
   memset(&server_addr, 0, sizeof(server_addr));
   server_addr.sin_family = AF_INET;
   server_addr.sin_addr.s_addr = htonl (INADDR_ANY);  /* This allows choice of
                                        any host interface, if more than one
                                        are present */ 
   server_port = SERV_UDP_PORT; /* Server will listen on this port */
   server_addr.sin_port = htons(server_port);

   /* bind the socket to the local server port */

   if (bind(sock_server, (struct sockaddr *) &server_addr,
                                    sizeof (server_addr)) < 0) {
      perror("Server: can't bind to local address");
      close(sock_server);
      exit(1);
   }                     
   
   /* user interface */
   printf("Please enter a timeout value from 1 to 10:\n");
   scanf("%d", &timeout_input);

   /* int and loop to perform exponentiation.
    * Had problems linking to math library on 
    * cisc450 machine.
    */
   int micro = 1;
   for(int i=0; i<timeout_input; i++) {
	micro = micro * 10;
   }
   /* dividing microseconds into s and us categories */
   timeout.tv_sec = micro / 1000000;
   timeout.tv_usec = micro % 1000000;

   printf("Timeout is %d seconds and %d us\n", timeout.tv_sec, timeout.tv_usec);
   printf("Please enter a packet loss ratio between 0 and 1:\n");
   scanf("%f", &packet_loss_ratio);

   /* No listen call because we are using a datagram service; not waiting
    * for connection requests, just waiting to receive a datagram
    */
   printf("Waiting for incoming messages on port %hu\n\n", server_port);
  
   client_addr_len = sizeof (client_addr);

   /* wait for incoming datagrams in an indefinite loop */

   for (;;) {

      /* receive the message */
	FILE *read_file = NULL;

	char seg_recv[GRAM_SIZE]; //char array for receiving header/filename payload
	bool end_of_file = false;
	int place = 0;
	short sequence = 0; // server starts sending at 0 with alternating between 0 and 1 
	char temp_read; // each character
	char seg_send[GRAM_SIZE]; //char array for sending
	short ack_recv;  //acks received are only 1 short for ACK num
	char eot_seg[4]; //specific size for EOT, no data bytes
	int data_total = 0; //initial transmission data bytes
	bool good_ack = true; //bool used for timeout waiting loop
	int count = 0; //total generated packets (trans and retrans)
	int og_count = 0; //original transmissions generated
	int success_count = 0; //number of packets successfully transmitted
	int drop_count = 0; //number of sent packets lost
	int ack_count = 0; // number of ACKs received
	int timeout_count = 0; //number of timeouts occurred

	/* Using NULL because since we are only running these programs locally and on our own port,
	 * we are not worried about incoming datagrams from other sources, so we don't filter based
	 * on sender address.
	 */
        bytes_recd = recvfrom(sock_server, seg_recv, GRAM_SIZE, 0, (struct sockaddr *) &client_addr, &client_addr_len); 
	printf("File requested: %s\n", &seg_recv[4]); // prints requested filename for correctness

      read_file = fopen(&seg_recv[4], "r"); //filename starts at 4th byte (bytes 0-3 are header vals)

      if (bytes_recd > 0){

	setsockopt(sock_server, SOL_SOCKET, SO_RCVTIMEO, (const void *) &timeout, sizeof(timeout)); //set timeout option on recvfrom

        /* prepare the message to send */

	/* structure of datagrams used:
	 * 	bytes 0 and 1 denote the payload size; are converted to/from short and 2 chars (2 bytes to 2 bytes)
	 * 	bytes 2 and 3 denote the sequence number
	 * 	bytes 4 to 83 constitute the payload
	 * 	NOTE: Since sequence numbers in ABP/Stop-And-Wait are always 0 or 1, we don't actually need 2 bytes.
	 * 	      We use two bytes with the upper byte as 0 anyways to conform to the lab document's listed
	 * 	      header structure.
	 */

	while (end_of_file == false){ // parse file line by line until EOF bool is activated
		temp_read = fgetc(read_file); //character by character to watch out for EOF and \n
		if (temp_read == EOF){ // end of file condition
			end_of_file = true;	
			eot_seg[0] = 0; //EOT header packet
			eot_seg[1] = 0; //both first bytes 0 because 0 data bytes
			eot_seg[2] = (unsigned char) sequence; //either 1 or 0
			eot_seg[3] = 0; //upper 8 bits of sequence are going to be 0s
			/* EOT only contains 4 bytes of header, no payload */
			sendto(sock_server, eot_seg, 4, 0, (struct sockaddr *) &client_addr, client_addr_len); //send the EOF packet, no data packet
			/* printing statistics for the file transmission */
			printf("\nEnd of Transmission Packet with sequence number %d transmitted\n\n", sequence);

			printf("Number of data packets generated for initial transmission: %d\n", og_count);
			printf("Total number of data bytes generated for initial transmission: %d\n\n", data_total);

			printf("Total number of data packets generated (including retransmissions): %d\n\n", count);

			printf("Number of data packets dropped due to loss: %d\n", drop_count);
			printf("Number of data packets successfully transmitted (including retransmissions): %d\n\n", success_count);

			printf("Number of ACKs received: %d\n", ack_count);
			printf("Number of times timeout expired: %d\n\n", timeout_count);

			
		} //if EOF
		else if (temp_read == '\n'){ //reads newline character (end of line), stops before null terminator (\0)
			seg_send[place] = '\n';
			place++;
			//sending
			msg_len = place + 1; //includes header space
			data_total += msg_len - 4; //keeping count of total original data sent
			// header setting
			char_conv.n = msg_len - 4;
			seg_send[0] = convert_to_char1(msg_len - 4); //copies first byte of header
			seg_send[1] = convert_to_char2(msg_len - 4); //copies second byte of header
			seg_send[2] = (unsigned char) sequence; //copies bottom 8 of sequence num
			seg_send[3] = 0; //sequence num is 0 or 1 so upper 8 of sequence num is going to be 0
			printf("Packet %d generated for transmission with %d data bytes\n", sequence, msg_len-4); // minus 4 to only print data bytes
			og_count++;
			count++;
			if (!simulate_loss(packet_loss_ratio)) { //if packet is not designated to be dropped
				success_count++;
         			bytes_sent = sendto(sock_server, seg_send, msg_len, 0, (struct sockaddr *) &client_addr, client_addr_len);
				printf("\tPacket %d successfully transmitted with %d data bytes\n", sequence, bytes_sent - 4);
			}
			else { //if packet is designated to be dropped
				drop_count++;
				printf("\tPacket %d lost\n", sequence);
				
			}
			good_ack = false; //false to enter loop to wait for correct ACK/handle timeout
			/* while loop for timeouts/retransmissions */
			while (!good_ack){
				bytes_recd = recvfrom(sock_server, &ack_recv, sizeof(short), 0, (struct sockaddr *) NULL, NULL); 
				if (bytes_recd > 0 && ack_recv == sequence) { //ack received and correct ack num
					ack_count++;
					printf("\tACK %d received\n", sequence);
					good_ack = true; //correct ACK received; breaks loop
				}
				else if (bytes_recd > 0){
					ack_count++;
					printf("\tACK %d received\n", ack_recv);
				}
				else {
					timeout_count++;
					printf("\tTimeout expired for packet numbered %d\n", sequence);
					printf("\tPacket %d generated for re-transmission with %d data bytes\n", sequence, msg_len-4);
					count++;
					//timeout occurred or incorrect ack num
					//retransmit
					if(!simulate_loss(packet_loss_ratio)){
						success_count++;
         					bytes_sent = sendto(sock_server, seg_send, msg_len, 0, (struct sockaddr *) &client_addr, client_addr_len);
						printf("\tPacket %d successfully transmitted with %d data bytes\n",sequence, bytes_sent - 4);
					}
					else{
						drop_count++;
						printf("\tPacket %d lost\n", sequence);
					}

				}
			} //while(!good_ack)
			sequence = 1 - sequence;  // flip sequence num
			place = 4; //starts reading chars into array at 4 to leave space for 4 byte header
			memset(seg_send, 0, GRAM_SIZE); //clears entire array to start reading new line into it
			msg_len = 0;
		} //else if new line character found (reached end of line)
		else {   //read a normal character, place into the array and increment index
			seg_send[place] = temp_read;
			place++;
		}
	} //while not EOF
      } //if received file name correctly
      /* close the socket */
      close(sock_server);
      break; //break the endless loop; end of program
   } //indefinite for loop
} //main
