CFLAGS=-Wall
sws: main.c net.c net.h sws.h http.c http.h log.c cgi.c error.c utils.c
	cc $(CFLAGS) -o sws main.c net.c http.c log.c cgi.c error.c utils.c -g
clean:
	rm -f sws 
