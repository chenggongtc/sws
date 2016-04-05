/*
 * net.c		- server setup program for `sws` command
 * sws			- a simple web server
 *
 * CS631 final project	- sws
 * Group 		- flag
 * Members 		- Chenyao Wang, cwang60@stevens.edu
 *			- Dongxu Han, dhan7@stevens.edu
 *			- Gong Cheng, gcheng2@stevens.edu
 *			- Maisi Li, mli27@stevens.edu
 */


#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sws.h"
sigjmp_buf sig_buf;
/*handle SIGHUP to restart program*/
static void 
sig_usr(int sig) {
	if(sig == SIGHUP) {
		parse_conf(&server_info);
		siglongjmp(sig_buf,1);
	}
	return;
}


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
	int status_code;
	socklen_t length;
	pid_t p;
	RESPONSE resp;
	REQUEST req;
	int sig_val;

	on = 1;
	sock = 1;
	sig_val = sigsetjmp(sig_buf,1);	
	if(sig_val != 0) {
		if(sock > 0)
			close(sock);
		port = server_info.si_port;
/*		if(strcmp(server_info.si_address, "") == 0)
			server_addr = NULL;
		else
			server_addr = server_info.si_address;
*/	strncpy(doc_root, server_info.si_rootdir, sizeof doc_root);
		if(strcmp(server_info.si_cflag, "TRUE") == 0)
			strncpy(cgi_dir, server_info.si_cgidir, sizeof cgi_dir);
		if(strcmp(server_info.si_lflag, "TRUE") == 0)
			strncpy(log_file, server_info.si_logpath, sizeof log_file);	
	}

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
	signal(SIGCHLD, SIG_IGN);
	signal(SIGHUP, sig_usr);
	/* Set main socket to non-blocking mode */
	if ((sock_flag = fcntl(sock, F_GETFL, 0)) == -1) {
		perror("fcntl() error");
		exit(EXIT_FAILURE);
	}

	if (fcntl(sock, F_SETFL, sock_flag | O_NONBLOCK) == -1) {
		perror("fcntl() error");
		exit(EXIT_FAILURE);
	}

	if (!dflag) {
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
			status_code = req_parser(msgsock, &req);

			int target_fd;
			char * res_val;

			ini_response(&resp);
			resp.status = status_code;
			if ( (res_val =res_builder(&resp, &req, &target_fd, 
				(char *)&client_ip)) == NULL){
				close(msgsock);
				continue;
           		 }
           		if (res_send(res_val, target_fd, msgsock) != 0){
             			   //perror("res send error");
         		}

			log_info.l_req_stat = resp.status;
			log_sws(&log_info);
			close(msgsock);
			/* test end */
		} else {
			if ((p = fork()) < 0) {
				close(msgsock);
			} else if (p == 0) {	/* child */
				ini_request(&req);
				status_code = req_parser(msgsock, &req);
				int target_fd;
				char * res_val;

				ini_response(&resp);
				resp.status = status_code;
				if ((res_val =res_builder(&resp, &req, 
					&target_fd,
					(char *)&client_ip)) == NULL){
					close(msgsock);
					continue;
           			 }
           			if (res_send(res_val, target_fd, msgsock) != 0){
             			   //perror("res send error");
         			}

				log_info.l_req_stat = resp.status;
				log_sws(&log_info);
				close(msgsock);
				_exit(EXIT_SUCCESS);
			} else	/* parent */
				close(msgsock);
		}
	}
}


