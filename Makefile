CC=g++
CFLAGS=-Wall -O3 -g
LFLAGS=-lgflags -lrt

trafgen: main.o client.o
	$(CC) main.o client.o $(LFLAGS) -o $@

main.o: main.cc common.h
	$(CC) -c $(CFLAGS) main.cc -o $@

client.o: client.cc common.h
	$(CC) -c $(CFLAGS) client.cc -o $@

clean:
	rm -rf trafgen *.o
