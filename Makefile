all: udpclient udpserver

udpclient: udpclient.c
	gcc -o udpclient udpclient.c

udpserver: udpserver.c
	gcc -o udpserver udpserver.c

clean:
	rm -f udpserver udpclient out.txt
