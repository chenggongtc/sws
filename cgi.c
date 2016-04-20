#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/wait.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sws.h"

extern char **environ;

void
ini_cgi(CGI_ENV *cgi)
{
	struct utsname u;

	bzero(cgi->content_length, sizeof(cgi->content_length));
	cgi->content_type = CONTENT_TYPE;
	cgi->gateway_interface = CGI_GATEWAY;
	bzero(cgi->query_string, sizeof(cgi->query_string));
	bzero(cgi->remote_addr, sizeof(cgi->remote_addr));
	cgi->request_method = NULL ;
	bzero(cgi->path_info, sizeof(cgi->path_info));
	bzero(cgi->script_name, sizeof(cgi->script_name));
	if (uname(&u) == -1)
		bzero(cgi->server_name, sizeof(cgi->server_name));
	else
		strcpy(cgi->server_name, u.nodename);
	cgi->server_port = port;
	cgi->server_protocol = NULL;
	cgi->server_software = SERVER_NAME;
}

void
cgi_parser(CGI_ENV *cgi, REQUEST *req, char *client_ip)
{
	int i;
	int len;

	if (req->status != STAT_NOT_SPECIFIED || req->is_cgi == NOT_SPECIFIED) 
		return;

	len = strlen(req->query);
	if (len >= CONTENT_BUFSIZE) {
		req->status = STAT_BAD_REQUEST;
		return;
	}

	sprintf(cgi->content_length, "%d", len);
	strcpy(cgi->query_string, req->query);
	strcpy(cgi->remote_addr, client_ip);

	switch (req->method) {
	case HEAD:
		cgi->request_method = "HEAD";
		break;
	case GET:
		cgi->request_method = "GET";
		break;
	default:
		req->status = STAT_BAD_REQUEST;
		return;
	}

	switch (req->ver) {
	case HTTP09:
		cgi->server_protocol = "HTTP/0.9";
		break;
	case HTTP10:
		cgi->server_protocol = "HTTP/1.0";
		break;
	default:
		req->status = STAT_BAD_REQUEST;
		return;
	}

	len = strlen(req->uri);
	for (i = strlen(cgi_dir); i < len; i++) {
		if (req->uri[i] == '/')
			break;
	}
	strcpy(cgi->script_name, "/cgi-bin/");
	strncat(cgi->script_name, req->uri + strlen(cgi_dir), i - 
					strlen(cgi_dir));

	if (i < len)
		strcpy(cgi->path_info, req->uri + i);
}

/*
 * Return STAT_NOT_SPECIFIED on success, or the corresponding status code on
 * error. 
 *
 * The read end of the pipe is stored in the argument fd.  It needs to be
 * closed by the caller of the function.
 */
void
cgi_exec(int msgsock, CGI_ENV *cgi)
{
	pid_t pid;
	struct stat st;
	char *a[]={NULL};
	char path[PATH_MAX];
	int fds[2];
	int ch;

	bzero(path, sizeof(path));

	sprintf(path, "%s%s", cgi_dir, cgi->script_name + 9);
	if (stat(path, &st) == -1) {
		return;
	}

	if (!S_ISREG(st.st_mode)) {
		return;
	}

	if (access(path, X_OK) == -1) {
		return;
	}

	if (pipe(fds) < 0) {
		return;
	}

	if ((pid = fork()) == -1) {
		return;
		
	} else if (pid > 0) {	/* parent */
		close(fds[1]);

		/*
		if (dup2(fds[0], msgsock) == -1)
			return;
		*/

		if (waitpid(pid, NULL, 1) < 0) {
			return;
		}
		
		while (read(fds[0], &ch, 1) > 0)
			write(msgsock, &ch, 1);

	} else {	/* child */
		close(fds[0]);

		if (dup2(fds[1], STDOUT_FILENO) == -1) {
			exit(EXIT_FAILURE);
		}

		if (setenv("CONTENT_LENGTH", cgi->content_length, 1) == -1 ||
		setenv("QUERY_STRING", cgi->query_string, 1) == -1 ||
		setenv("REQUEST_METHOD", cgi->request_method, 1) == -1 ||
		setenv("REMOTE_ADDR", cgi->remote_addr, 1) == -1 ||
		setenv("CONTENT_TYPE", cgi->content_type, 1) == -1 ||
		setenv("GATEWAY_INTERFACE", cgi->gateway_interface, 1) == -1 ||
		setenv("PATH_INFO", cgi->path_info, 1) == -1 ||
		setenv("SCRIPT_NAME", cgi->script_name, 1) == -1 ||
		setenv("SERVER_NAME", cgi->server_name, 1) == -1 ||
		setenv("SERRVER_PORT", cgi->server_port, 1) == -1 ||
		setenv("SERVER_PROTOCOL", cgi->server_protocol, 1) == -1||
		setenv("SERVER_SOFTWARE", cgi->server_software, 1) == -1) {
			return;
		}

		execve(path, a, environ);
		exit(EXIT_FAILURE);
	}
}



