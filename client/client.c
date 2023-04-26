/**
 * filename: client.c
 * Author: Aarushi Roy Choudhury
 * Last Updated: 2023-03-22
 */


#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <dirent.h>

#define BUFF_SIZE 1024	//Packet size = 1024 bytes

//utility function to display error messages
static void displayError(const char *message)
{
	perror(message);
	exit(EXIT_FAILURE);
}


//Create a packet structure with an ID, length and data
struct packet {
	long int ID;
	long int length;
	char data[BUFF_SIZE];
};


int main(int argc, char **argv)
{
	if ((argc < 3) || (argc >3)) {
		printf("Must enter [IP Address] [Port Number]\n");  // check command line args are entered: IP and Port
	}

	char commandSent[50];
	char filename[20];
	char command[10];
	
	
	ssize_t length = 0;
	off_t fileSize = 0;
	long int ackNum = 0;
	
	struct stat st;
	struct packet packet;
	struct timeval timeOut = {0,0}; //intialize timeout values with 0 sec and 0 usec

	
	struct sockaddr_in clientAddress;
	struct sockaddr_in serverAddress;

	int clientSocket;

	FILE* filePointer;

	
	clientAddress.sin_family = AF_INET; // set address type to Internet Protocol (IP) v4 Addresses
	clientAddress.sin_port = htons(atoi(argv[2])); //Port no from command line
	clientAddress.sin_addr.s_addr = inet_addr(argv[1]); //IP address from command line

	//create client socket
	if ((clientSocket = socket(AF_INET, SOCK_DGRAM, 0)) == -1) //socket (domian, type, protocol) protocol can be left as 0 if default protocol of type is used
		displayError("Client socket failed.");

	while(1) { //infinite loop


		printf("Enter a command: \n download [FILENAME] \n or \n upload [FILENAME]\n");	//display user command menu	
		scanf(" %[^\n]%*c", commandSent); //get input
		//%*[^\n] scans everything until a \n, but doesn't scan in the \n. The asterisk(*) tells it to discard whatever was scanned.
		//%*c scans a single character, which will be the \n left over by %*[^\n] in this case. The asterisk instructs scanf to discard the scanned character.

		
		sscanf(commandSent, "%s %s", command, filename); //put input into command and filename

		if (sendto(clientSocket, commandSent, sizeof(commandSent), 0, (struct sockaddr *) &clientAddress, sizeof(clientAddress)) == -1 ) {
			displayError("Client sending error");
		}

//===================================================================== Download from server ====================================================================================
		//in this case server is transmitting and client is recieving.

		if ((strcmp(command, "download") == 0) && (filename[0] != '\0' )) { //conitnue if the command == download and filename is not null

			long int fullPacket = 0;
			long int recievedBytes = 0, i = 0;

			timeOut.tv_sec = 2;
			setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeOut, sizeof(struct timeval)); //Set timeout of 2 seconds if server does not respond
			//level = SOL_SOCKET, the item will be searched for in the socket itself.
			// SO_RCVTIMEO option defines the receive timeout value
			
			recvfrom(clientSocket, &(fullPacket), sizeof(fullPacket), 0, (struct sockaddr *) &serverAddress, (socklen_t *) &length); //Get the total number of packets from server

			timeOut.tv_sec = 0;
            setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeOut, sizeof(struct timeval)); //Turn off timeout after response from server is recieved
			
			if (fullPacket > 0) {
				sendto(clientSocket, &(fullPacket), sizeof(fullPacket), 0, (struct sockaddr *) &clientAddress, sizeof(clientAddress)); //comment out when you want to test retransmission
				
				//Demo retransmission for download from server in client.c

 				// uncomment below to demo retransmission. Be sure to comment out sendto() above
                    //==================================================================================================================================================
                    // int counter = 1;  //Send acks in sequence, skip packet if counter is a mutliple of 5, skips every 5th packet.
                    // if (counter % 5 != 0) {
                    //  sendto(clientSocket, &(packet.ID), sizeof(packet.ID), 0, (struct sockaddr *) &serverAddress, sizeof(serverAddress)); //send the ack
                    // }
                    // counter++;
				//======================================================================================================================================================
				printf("Total no. of packets from server:  %ld\n", fullPacket);
				
				filePointer = fopen(filename, "wb"); //open the recieved file in write mode

				//Recieve all packets 
				//Send the acks in sequence
				for (i = 1; i <= fullPacket; i++)
				{
					memset(&packet, 0, sizeof(packet));

					recvfrom(clientSocket, &(packet), sizeof(packet), 0, (struct sockaddr *) &serverAddress, (socklen_t *) &length);  //get the packet
					sendto(clientSocket, &(packet.ID), sizeof(packet.ID), 0, (struct sockaddr *) &clientAddress, sizeof(clientAddress)); //send the ack

				
					if ((packet.ID < i) || (packet.ID > i)) //drop repeated packets
						i--;
					else {
						fwrite(packet.data, 1, packet.length, filePointer);  //write back data into the open file on the client
						printf("packet  %ld	\n", packet.ID);
						recievedBytes += packet.length;
					}

					if (i == fullPacket) {
						printf("File recieved from server successfully\n");
					}
				}
				printf("Total number of bytes recieved  %ld\n", recievedBytes);
				fclose(filePointer);
			}
			else {
				printf("File doesn't exist or the file is empty\n");
			}
		}

//========================================================================== Upload to server =======================================================================================

		//in this case client is transmitting and server is recieving.

		else if ((strcmp(command, "upload") == 0) && (filename[0] != '\0')) { //conitnue if the command == upload and filename is not null
			
			if (access(filename, F_OK) == 0) {	//Check if the file is the directory

				int fullPacket = 0, retransmitPacket = 0, dropPacket = 0, flagTimeOut = 0;
				long int i = 0;

				stat(filename, &st); //calculate file size using stat()
				fileSize = st.st_size;	

				timeOut.tv_sec = 2;
				setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeOut, sizeof(struct timeval)); //Set timeout of 2 seconds for a response

				filePointer = fopen(filename, "rb"); //open the file that needs to be sent in read mode

				//calculate total number of packets needed to transmit the file
				if ((fileSize % BUFF_SIZE) == 0)

					fullPacket = (fileSize / BUFF_SIZE); //If packet can be divided evenly by 1024 bytes

				else
					fullPacket = (fileSize / BUFF_SIZE) + 1; //If packet cannot be divided evenly by 1024 bytes then add 1 more packet


				printf("Total number of packets  %d	\n", fullPacket);

				sendto(clientSocket, &(fullPacket), sizeof(fullPacket), 0, (struct sockaddr *) &clientAddress, sizeof(clientAddress)); //Send number of packets to reciever
				recvfrom(clientSocket, &(ackNum), sizeof(ackNum), 0, (struct sockaddr *) &serverAddress, (socklen_t *) &length); //Get acknowledgements

				printf("Ack num  %ld\n", ackNum);

				
				while (ackNum != fullPacket) //keep trying to send packets and get acknowldegements until the ackNum matches the total num of packets
				{
					
					sendto(clientSocket, &(fullPacket), sizeof(fullPacket), 0, (struct sockaddr *) &clientAddress, sizeof(clientAddress));
					recvfrom(clientSocket, &(ackNum), sizeof(ackNum), 0, (struct sockaddr *) &serverAddress, (socklen_t *) &length); 
					
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

					sendto(clientSocket, &(packet), sizeof(packet), 0, (struct sockaddr *) &clientAddress, sizeof(clientAddress));   //send the packet
                    recvfrom(clientSocket, &(ackNum), sizeof(ackNum), 1, (struct sockaddr *) &serverAddress, (socklen_t *) &length); //get the acknowledgement

					
					while (ackNum != packet.ID)  //Check for ack
					{
						//keep trying to send packets and get acknowldegements until the ackNum matches
						sendto(clientSocket, &(packet), sizeof(packet), 0, (struct sockaddr *) &clientAddress, sizeof(clientAddress));
						recvfrom(clientSocket, &(ackNum), sizeof(ackNum), 0, (struct sockaddr *) &serverAddress, (socklen_t *) &length);

						//print out how many times a packet is dropped
						printf("packet  %ld	dropped, %d times\n", packet.ID, ++dropPacket);
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

					printf("packet  %ld	Ack  %ld\n", i, ackNum);

					if (fullPacket == ackNum)
						printf("File sent to server successfully.\n");
				}
				fclose(filePointer);
				
				timeOut.tv_sec = 0;
				setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeOut, sizeof(struct timeval)); //Turn off timeout after response from server is recieved
			}
		}

//======================================================================== Invalid Command ===========================================================================================

		else {
			printf("Client: Invalid input. Options are:\n download [FILENAME] \n or \n upload [FILENAME]");
		}


	}
		
	close(clientSocket);

	exit(EXIT_SUCCESS);
}
