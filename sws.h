#ifndef _SWS_H_
#define _SWS_H_
#include <ctype.h>
#include <errno.h>
#include <netinet/in.h>

#include <inttypes.h>
#include <limits.h>
#include <sys/file.h>
#include <time.h>

#define SERVER_NAME "fws/1.0"
#define CONTENT_TYPE "text/html"
#define CGI_GATEWAY "CGI/1.1"

#define SERVER_NAME "fws/1.0"
#define CONTENT_TYPE "text/html"
#define CGI_GATEWAY "CGI/1.1"

#define LWS " "
#define CRLF "\r\n"
#define SP 32
#define MAX_FILE_NAME 256
#define MAX_REQUEST_LEN 8192
#define URI_BUFSIZE 4096
#define CONTENT_BUFSIZE 2048
#define NOT_SPECIFIED 0
#define SPECIFIED 1
#define FILETYPE 0
#define DIRTYPE 1

#define STAT_NOT_SPECIFIED 0
#define STAT_OK 200
#define STAT_CREATED 201
#define STAT_ACCEPTED 202
#define STAT_NO_CONTENT 204
#define STAT_MOVED_PERMANENTLY 301
#define STAT_MOVED_TEMPORARILY 302
#define STAT_NOT_MODIFIED 304
#define STAT_BAD_REQUEST 400
#define STAT_UNAUTHORIZED 401
#define STAT_FORBIDDEN 403
#define STAT_NOT_FOUND 404
#define STAT_REQUEST_TIMEOUT 408
#define STAT_INTERNAL_SERVER_ERROR 500
#define STAT_NOT_IMPLEMENTED 501
#define STAT_BAD_GATEWAY 502
#define STAT_SERVICE_UNAVAILABLE 503

#ifdef __linux__
#define EBLOCK EWOULDBLOCK
#else
#define EBLOCK EAGAIN
#endif

extern int cflag;
extern int dflag;
extern int iflag;
extern int lflag;
extern char doc_root[PATH_MAX];
extern char cgi_dir[PATH_MAX], log_file[PATH_MAX];
extern char *server_addr, *port;

typedef enum {
	GET,
	HEAD,
	UNKNOW_METHOD
} METHOD;

typedef enum {
	HTTP09,
	HTTP10,
	UNKNOW_VER
} VER;

typedef struct {
	METHOD method;
	int status;
	char uri[URI_BUFSIZE];
	char raw_uri[URI_BUFSIZE]; /* used for redirection */
	char query[URI_BUFSIZE];
	VER ver;
	int is_ims;	/* NOT_SPECIFIED, SPECIFIED */
	int is_cgi;
	struct tm if_modified_since; /* (GMT) */
} REQUEST;

typedef struct {
	char content_length[5];
	char *content_type;
	char *gateway_interface;
	char query_string[URI_BUFSIZE];
	char remote_addr[INET6_ADDRSTRLEN];
	char *request_method;
	char path_info[PATH_MAX];
	char script_name[PATH_MAX];
	char server_name[20];
	char *server_port;
	char *server_protocol;
	char *server_software;
} CGI_ENV;

typedef struct {
	METHOD method;
	int status;			/* status code, 3DIGIT */
	time_t curr_date;			/* current date(GMT) */
	char *server;
	time_t lm_date;		/* Last-Modified date(GMT) */
	char *content_type;
	uintmax_t content_len;		/* size in bytes of the data returned*/
	char content_path[PATH_MAX];	/* the path of the content */
	char raw_uri[URI_BUFSIZE]; /* used for redirection */
	int ls_type;		/* flag for dir list */
} RESPONSE;

typedef struct {
	int status;
	uintmax_t size;
	char l_addr[INET6_ADDRSTRLEN];
	char l_line[MAX_REQUEST_LEN];
	time_t l_time; /* Request time(GMT) */
} LOG;

extern LOG log_info;

void start_server();
//struct req_header req_parser(char *);

/* http.c */
void ini_request(REQUEST *);
void ini_response(RESPONSE *);
void req_parser(int, REQUEST *, LOG *);
void response(int, RESPONSE *, REQUEST *, CGI_ENV *, LOG *);

/* cgi.c */
void ini_cgi(CGI_ENV *);
void cgi_parser(CGI_ENV *, REQUEST *, char *);
void cgi_exec(int, CGI_ENV *); /* fd should be closed after calling */

/* log.c */
void ini_log(LOG *);
void log_sws(LOG *);

/* error.c */
int err_length(int);
void send_err_content(int, int);

/* utils.c */
int write_str(int, char *);
#endif


