all : server
objects = *.o

server : server.o
	cc -Wall -g -o server server.o

server.o : server.c
	cc -Wall -g $(INC) -c server.c

clean :
	rm -f server $(objects) *.txt *.log
