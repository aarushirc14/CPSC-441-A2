all : client
objects = *.o

client : client.o
	cc -Wall -o client client.o

client.o : client.c
	cc -Wall $(INC) -c client.c

clean :
	rm -f client $(objects) *.txt *.log
