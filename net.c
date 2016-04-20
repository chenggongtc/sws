#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "net.h"
#include "sws.h"

/* start routine of our web server */
void
start_server()
{
	int sock;
	int msgsock;
	/* return value for getaddrinfo() */
	int getVal;
	int on;
	int sock_flag;
	struct addrinfo *res, *iter, hints;
	struct sockaddr_in6 server6;
	/*
	 * struct sockaddr_in6 is large enough to store either IPv4 or IPv6
	 * address information
	 */
	struct sockaddr_in6 client;
	char client_ip[INET6_ADDRSTRLEN];
	socklen_t length;
	pid_t p;
	RESPONSE resp;
	REQUEST req;
	CGI_ENV cgi;
	LOG log_info;

	on = 1;

	bzero(client_ip, sizeof(client_ip));

	if (server_addr == NULL) {	/* listen all ipv4 and ipv6 addresses */
		if ((sock = socket(AF_INET6, SOCK_STREAM, 0)) == -1) {
			perror("socket() error");
			exit(EXIT_FAILURE);
		}

		if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&on,
				sizeof(on)) == -1) {
			perror("setsockopt() error");
			exit(EXIT_FAILURE);
		}

		memset(&server6, 0, sizeof(server6));

		server6.sin6_family = AF_INET6;
		server6.sin6_addr = in6addr_any;
		server6.sin6_port = htons(atoi(port));

		length = sizeof(server6);

		if (bind(sock, (struct sockaddr *)&server6, length)) {
			perror("bind() error");
			exit(EXIT_FAILURE);
		}

		if(listen(sock, SOMAXCONN) == -1) {
			perror("listen() error");
			exit(EXIT_FAILURE);
		}
	} else {	/* listen the given address */
		memset(&hints, 0, sizeof(hints));

		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;

		if ((getVal = getaddrinfo(server_addr, port, &hints,
					  &res)) == -1 ) {
			fprintf(stderr, "getaddrinfo() error: %s",
				gai_strerror(getVal));
			exit(EXIT_FAILURE);
		}

		for (iter = res; iter; iter = iter->ai_next) {
			if ((sock = socket(iter->ai_family, iter->ai_socktype,
					iter->ai_protocol)) == -1)
				continue;

			if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
					(char *)&on, sizeof(on)) == -1) {
				perror("setsockopt() error");
				exit(EXIT_FAILURE);
			}


			if (bind(sock, iter->ai_addr, iter->ai_addrlen)) {
				perror("bind() error");
				exit(EXIT_FAILURE);
			}

			break;	/* break on success */
		}

		if (iter == NULL) {
			fprintf(stderr, "Not a valid ip address.\n");
			exit(EXIT_FAILURE);
		}

		freeaddrinfo(res);

		if(listen(sock, SOMAXCONN) == -1) {
			perror("listen() error");
			exit(EXIT_FAILURE);
		}
	}

	/* prevent child processes from becoming zombies */
	if (signal(SIGCHLD, chldHandler) == SIG_ERR ||
		signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
		perror("signal() error");
		exit(EXIT_FAILURE);
	}

	/* Set main socket to non-blocking mode */
	if ((sock_flag = fcntl(sock, F_GETFL, 0)) == -1) {
		perror("fcntl() error");
		exit(EXIT_FAILURE);
	}

	if (fcntl(sock, F_SETFL, sock_flag | O_NONBLOCK) == -1) {
		perror("fcntl() error");
		exit(EXIT_FAILURE);
	}

	if (!dflag) { /* test */
		if (daemon(1, 0) == -1) {
			perror("daemon() error");
			exit(EXIT_FAILURE);
		}
	}

	for (;;) {

		length = sizeof(client);

ACCAGAIN:
		if ((msgsock = accept(sock, (struct sockaddr *) &client,
					&length)) < 0) {
			if (errno == EBLOCK)
				goto ACCAGAIN;
			continue;
		}

		if ((sock_flag = fcntl(msgsock, F_GETFL, 0)) == -1) {
			close(msgsock);
			continue;
		}

		if (fcntl(msgsock, F_SETFL, sock_flag | O_NONBLOCK) == -1) {
			close(msgsock);
			continue;
		}
		/*
		 * Because we use in6addr_any to listen all IPv4 and
		 * IPv6 addresses on the host, IPv4 addresses will be
		 * mapped into IPv6 addresses if -i is not specified.
		 */
		if (client.sin6_family == AF_INET) {
			inet_ntop(AF_INET,
			&((struct sockaddr_in *)&client)->sin_addr,
			client_ip, INET_ADDRSTRLEN);
		} else {
			inet_ntop(AF_INET6, &client.sin6_addr,
			client_ip, INET6_ADDRSTRLEN);
		}

		ini_log(&log_info);
		strcpy(log_info.l_addr, client_ip);

		if (dflag) {
			ini_request(&req);
			ini_response(&resp);
			ini_cgi(&cgi);

			req_parser(msgsock, &req, &log_info);
			cgi_parser(&cgi, &req, client_ip);
			response(msgsock, &resp, &req, &cgi, &log_info);

			log_sws(&log_info);
			
			close(msgsock);
		} else {
			if ((p = fork()) < 0) {
				close(msgsock);
			} else if (p == 0) {	/* child */
				ini_request(&req);
				ini_response(&resp);
				ini_cgi(&cgi);

				req_parser(msgsock, &req, &log_info);
				cgi_parser(&cgi, &req, client_ip);
				response(msgsock, &resp, &req, &cgi, &log_info);

				log_sws(&log_info);
				
				close(msgsock);
				_exit(EXIT_SUCCESS);
			} else	/* parent */
				close(msgsock);
		}
	}
}

static void
chldHandler(int signo) {
	int status;
	while (waitpid(-1, &status, WNOHANG) > 0) {
	}
}
