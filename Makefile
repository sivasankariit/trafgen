CC=g++
CFLAGS=-Wall -O3 -g
LFLAGS=-lgflags -lrt

trafgen: main.o client.o server.o sockutils.o
	$(CC) main.o client.o server.o sockutils.o $(LFLAGS) -o $@

main.o: main.cc common.h
	$(CC) -c $(CFLAGS) -std=c++11 main.cc -o $@

client.o: client.cc common.h
	$(CC) -c $(CFLAGS) -std=c++11 client.cc -o $@

server.o: server.cc common.h
	$(CC) -c $(CFLAGS) -std=c++11 server.cc -o $@

sockutils.o: sockutils.cc common.h
	$(CC) -c $(CFLAGS) -std=c++11 sockutils.cc -o $@

clean:
	rm -rf trafgen *.o
