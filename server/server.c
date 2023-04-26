/**
 * filename: server.c
 * Author: Aarushi Roy Choudhury
 * Last Updated: 2023-03-22
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <dirent.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>



#define BUFF_SIZE 1024	//Packet size = 1024 bytes

//utility function to display error messages
static void displayError(const char *message)
{
	perror(message);
	exit(EXIT_FAILURE);
}


//Create a packet structure with an ID, length and data
struct packet_t {
	long int ID;
	long int length;
	char data[BUFF_SIZE];
};


int main(int argc, char **argv)
{
	//check that user entered command line arg has a port number for the server
	if ((argc < 2) || (argc > 2)) {				
		printf("Must enter [Port Number]\n"); //enter a large port number not in use like 8000
		exit(EXIT_FAILURE);
	}

	FILE * filePointer;

	
	ssize_t numRead;
	ssize_t length;
	off_t fileSize; 	
	long int ackNum = 0;   
	int serverSocket;

	char filenameRecieved[20];         
	char commandRecieved[10];
	char messageRecieved[BUFF_SIZE];

	struct sockaddr_in serverAddress, clientAddress;
	struct stat st;

	//The unit of data passed between your routines and the network layer is a packet_t 
	//which includes a character string representing the data that is passed between the application layers and the following data that is used only by the transport layers:

	//an (integer) sequence number
	//an (integer) ACK number, and
	//an (integer) checksum number

	struct packet_t packet;
	
	struct timeval timeOut = {0, 0}; //intialize timeout values with 0 sec and 0 usec


	serverAddress.sin_family = AF_INET; // set address type to Internet Protocol (IP) v4 Addresses
	serverAddress.sin_port = htons(atoi(argv[1])); //use port number entered in command line
	serverAddress.sin_addr.s_addr = INADDR_ANY; //When INADDR_ANY is specified in the bind call, the socket will be bound to all local interfaces

	//create server socket
	if ((serverSocket = socket(AF_INET, SOCK_DGRAM, 0)) == -1) //socket (domian, type, protocol) protocol can be left as 0 if default protocol of type is used
		displayError("Server socket failed.");

	if (bind(serverSocket, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) == -1)
		displayError("Error binding server socket");

	//Server runs on infinite loop to recieve client requests
	while(1) {
		printf("Server is ready for client connection\n");

		length = sizeof(clientAddress);

		if((numRead = recvfrom(serverSocket, messageRecieved, BUFF_SIZE, 0, (struct sockaddr *) &clientAddress, (socklen_t *) &length)) == -1 ) //recieve message from client
			displayError("Server: error recieving");

		
		printf("Server: recieved message:  %s\n", messageRecieved);

		sscanf(messageRecieved, "%s %s", commandRecieved, filenameRecieved); //put message from client into commandRecieved and filenameRecieved

//===================================================================== Download ====================================================================================

		//in this case server is transmitting and client is recieving.

		if ((strcmp(commandRecieved, "download") == 0) && (filenameRecieved[0] != '\0')) { //conitnue if the command == download and filename is not null

			printf("Server: file to be downloaded is:  %s\n", filenameRecieved);

			if (access(filenameRecieved, F_OK) == 0) {	//Check if the file is the directory
				
				int fullPacket = 0, retransmitPacket = 0, dropPacket = 0, flagTimeOut = 0;
				long int i = 0;
					
				stat(filenameRecieved, &st);
				fileSize = st.st_size;		 //calculate file size using stat()	
				timeOut.tv_sec = 2;			
				setsockopt(serverSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeOut, sizeof(struct timeval));  //Set timeout of 2 seconds if client does not respond
				//level = SOL_SOCKET, the item will be searched for in the socket itself.
				// SO_RCVTIMEO option defines the receive timeout value

				filePointer = fopen(filenameRecieved, "rb"); //open the file that needs to be sent in read mode
				
				//calculate total number of packets needed to transmit the file
				if ((fileSize % BUFF_SIZE) == 0)
					fullPacket = (fileSize / BUFF_SIZE); //If packet can be divided evenly by 1024 bytes
				else
					fullPacket = (fileSize / BUFF_SIZE) + 1; //If packet cannot be divided evenly by 1024 bytes then add 1 more packet

				printf("Total number of packets: %d\n", fullPacket);
					
				length = sizeof(clientAddress);

				sendto(serverSocket, &(fullPacket), sizeof(fullPacket), 0, (struct sockaddr *) &clientAddress, sizeof(clientAddress));	//Send number of packets to reciever
				recvfrom(serverSocket, &(ackNum), sizeof(ackNum), 0, (struct sockaddr *) &clientAddress, (socklen_t *) &length);  //Get acknowledgements


				while (ackNum != fullPacket) //keep trying to send packets and get acknowldegemtns until the ackNum matches
				{
					
					sendto(serverSocket, &(fullPacket), sizeof(fullPacket), 0, (struct sockaddr *) &clientAddress, sizeof(clientAddress)); 
					recvfrom(serverSocket, &(ackNum), sizeof(ackNum), 0, (struct sockaddr *) &clientAddress, (socklen_t *) &length);

					retransmitPacket++;

					//If it fails after 10 tries then set time out flag to true
					if (retransmitPacket == 10) {
						flagTimeOut = 1;
						break;
					}
				}

				// First transmit packets in sequence
				// Then match acks
				for (i = 1; i <= fullPacket; i++) 
				{
					memset(&packet, 0, sizeof(packet));
					ackNum = 0;
					packet.ID = i;
					packet.length = fread(packet.data, 1, BUFF_SIZE, filePointer);

					sendto(serverSocket, &(packet), sizeof(packet), 0, (struct sockaddr *) &clientAddress, sizeof(clientAddress));	 //send the packet
					recvfrom(serverSocket, &(ackNum), sizeof(ackNum), 0, (struct sockaddr *) &clientAddress, (socklen_t *) &length); //get the acknowledgement

					while (ackNum != packet.ID)  //Check for ack
					{
						//keep trying to send packets and get acknowldegemtns until the ackNum matches
						sendto(serverSocket, &(packet), sizeof(packet), 0, (struct sockaddr *) &clientAddress, sizeof(clientAddress));
						recvfrom(serverSocket, &(ackNum), sizeof(ackNum), 0, (struct sockaddr *) &clientAddress, (socklen_t *) &length);

						// print out how many times a packet is dropped
						printf("packet: %ld	dropped, %d times\n", packet.ID, ++dropPacket);
						
						retransmitPacket++;

						//If retransmission fails after 5 tries then set time out flag to true
						if (retransmitPacket == 5) {
							flagTimeOut = 1;
							break;
						}
					}

					retransmitPacket = 0;
					dropPacket = 0;

					//Failure if timeout happens
					if (flagTimeOut == 1) {
						printf("A timeout has occured. File could not be transferred.\n");
						break;
					}

					printf("packet  %ld	ack   %ld \n", i, ackNum);

					if (fullPacket == ackNum)
						printf("File sent to client successfully.\n");
				}
				fclose(filePointer);

				timeOut.tv_sec = 0;
				setsockopt(serverSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeOut, sizeof(struct timeval)); //Turn off timeout after response from server is recieved
			}
			else {	
				printf("Invalid filename or file does not exist.\n");
			}
		}

//========================================================================== Upload =======================================================================================

		//in this case client is transmitting and server is recieving.
		
		else if ((strcmp(commandRecieved, "upload") == 0) && (filenameRecieved[0] != '\0')) {

			printf("Server: file to be uploaded is  %s\n", filenameRecieved);

			long int fullPacket = 0, recievedBytes = 0, i = 0;
			
			timeOut.tv_sec = 2;
			setsockopt(serverSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeOut, sizeof(struct timeval));   //Set timeout of 2 seconds if client does not respond
			//level = SOL_SOCKET, the item will be searched for in the socket itself.
			// SO_RCVTIMEO option defines the receive timeout value

			recvfrom(serverSocket, &(fullPacket), sizeof(fullPacket), 0, (struct sockaddr *) &clientAddress, (socklen_t *) &length);  //Get the total number of packets recieved from client
			
			timeOut.tv_sec = 0;
			setsockopt(serverSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeOut, sizeof(struct timeval)); //Turn off timeout after response from server is recieved
			
			if (fullPacket > 0) {
				sendto(serverSocket, &(fullPacket), sizeof(fullPacket), 0, (struct sockaddr *) &clientAddress, sizeof(clientAddress));

				// uncomment below to demo retransmission. Be sure to comment out sendto() above
                    //==================================================================================================================================================
                    // int counter = 1;  //Send acks in sequence, skip packet if counter is a mutliple of 5, skips every 5th packet.
                    // if (counter % 5 != 0) {
                    //  sendto(serverSocket, &(fullPacket), sizeof(fullPacket), 0, (struct sockaddr *) &clientAddress, sizeof(clientAddress)); //send the ack
                    // }
                    // counter++;
				//======================================================================================================================================================
				printf("Total packets from client: %ld\n", fullPacket);
	
				filePointer = fopen(filenameRecieved, "wb"); //open the recieved file in write mode

				//Recieve all packets 
				//Send the acks in  sequence
				for (i = 1; i <= fullPacket; i++)
				{
					memset(&packet, 0, sizeof(packet));

					recvfrom(serverSocket, &(packet), sizeof(packet), 0, (struct sockaddr *) &clientAddress, (socklen_t *) &length); //get the packet
				    sendto(serverSocket, &(packet.ID), sizeof(packet.ID), 0, (struct sockaddr *) &clientAddress, sizeof(clientAddress));   //send the ack

				
					if ((packet.ID < i) || (packet.ID > i)) { //drop repeated packets
						i--;
					}
					else {
						fwrite(packet.data, 1, packet.length, filePointer);   //write back data into the open file on the server
						printf("packet  %ld	\n", packet.ID);
						recievedBytes += packet.length;   
					}
					
					if (i == fullPacket)
						printf("File recieved from client successfully.\n");
				}
			       printf("Total number of bytes recieved  %ld\n", recievedBytes);
			       fclose(filePointer);
			}
			else {
				printf("File doesn't exist or the file is empty\n");
			}
		}

//======================================================================== Invalid Command ===========================================================================================

		else {
			printf("Invalid command entered.\n");
		}
	}
	
	close(serverSocket);
	exit(EXIT_SUCCESS);
}
