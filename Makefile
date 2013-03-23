CC=g++
CFLAGS=-Wall -O3 -g
LFLAGS=-lgflags -lrt

trafgen: main.o client.o server.o sockutils.o
	$(CC) main.o client.o server.o sockutils.o $(LFLAGS) -o $@

main.o: main.cc common.h
	$(CC) -c $(CFLAGS) main.cc -o $@

client.o: client.cc common.h
	$(CC) -c $(CFLAGS) client.cc -o $@

server.o: server.cc common.h
	$(CC) -c $(CFLAGS) server.cc -o $@

sockutils.o: sockutils.cc common.h
	$(CC) -c $(CFLAGS) sockutils.cc -o $@

clean:
	rm -rf trafgen *.o
