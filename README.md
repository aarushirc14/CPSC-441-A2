This program is an implementation of UDP and EFTP (Easy File Transfer Protocol). The objective was to combine UDP and reliable data transfer techniques from TCP to create a program that can reliably transmit files between a client and a server. Downloading from the server to the client and uploading from the client to the server is supported. Binary files with .zip, .mp4 and .mp3 extensions are supported along with simple .txt files. To test the program add some files under the client and server directories and use the instructions below to upload/download files on a LINUX machine. Test files are provided.

Retransmission and timeouts are supported. The current version of the program only supports a single client.

 

**Running the Program-**

Split your terminal. Run server on one side, client on the other.

Server:

    cd server

    make

    ./server [PORT NUMBER]

Note: use some port number not in use like 8000


Client:

    cd server

    make

    ./client [IP ADDRESS] [PORT]

2 user options:

    upload [FILENAME]

or

    download [FILENAME]

**Notes-**

Use the same port number as server.
To find your IP address use the Linux command:

    ifconfig  
    
 and use the number with the “inet” tag (Internet Protocol (IP) v4 Addresses). 

If you try to upload or download files that already exist in the client or server directory then the program will give an error.


**To Test Timeouts-**

First generate a large file in either client or server directory like

    truncate -s 200M largefile.bin

Then upload/download largefile.bin

While the file transmission is happening kill the receiver. The sender will try to retransmit 5 times and then a timeout will happen.


**To Test Retransmission-**

Uncomment the code in client.c or server.c for the retransmission section. Be sure to comment out the sendto() call above.

Uncommenting the code means ACKs will be skipped every 5th packet and you can see how the sender will retransmit until the packet is ACKed by the receiver.


**Checking Integrity of Files after Transmission-**

Want to know if your transmissions are corrupting the files? 

After the transmission tasks are completed, use Linux command

    shasum [FILENAME] 
    
to compute the SHA-1 digest of all files in both client’s directory and server’s directory to check integrity. The shasum value of the original file should match the transmitted one.


**Helpful Resources-**

Check out the textbook and videos by Prof Jim Kurose:

http&#x3A;//gaia.cs.umass.edu/kurose_ross/online_lectures.htm

 
