CFLAGS=-Wall
linux: main.c net.c sws.h http.c http.h log.c cgi.c error.c
	cc $(CFLAGS) -o sws main.c net.c http.c log.c cgi.c file_operation.c error.c -lbsd  -g
bsd: main.c net.c sws.h http.c http.h log.c cgi.c error.c
	cc $(CFLAGS) -o sws main.c net.c http.c log.c cgi.c file_operation.c error.c
clean:
	rm -f sws 
