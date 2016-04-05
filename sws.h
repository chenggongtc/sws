/*
 * sws.h		- header file for `sws` command, containing all
			  function and global variable forward declarations
 * sws			- a simple web server
 *
 * CS631 final project	- sws
 * Group 		- flag
 * Members 		- Chenyao Wang, cwang60@stevens.edu
 *			- Dongxu Han, dhan7@stevens.edu
 *			- Gong Cheng, gcheng2@stevens.edu
 *			- Maisi Li, mli27@stevens.edu
 */

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
#define STAT_SERVER_UNAVAILABLE 503
#define STAT_DIR_LIST	600

#ifdef __linux__
#define EBLOCK EWOULDBLOCK
#else
#define EBLOCK EAGAIN
#endif

extern int cflag;
extern int dflag;
extern int iflag;
extern int lflag;
extern int fd_log;
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
	char uri[URI_BUFSIZE];
	char raw_uri[URI_BUFSIZE];
	char query[URI_BUFSIZE];
	VER ver;
	int is_ims;	/* NOT_SPECIFIED, SPECIFIED */
	int is_cgi;
	struct tm if_modified_since;
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
	struct tm date;			/* current date */
	char *server;
	//chenyao change the timespec
	struct tm lm_date;		/* Last-Modified date */
	char *content_type;
	uintmax_t content_len;		/* size in bytes of the data returned*/
	char content_path[PATH_MAX];	/* the path of the content */
	int ls_type;		/* flag for dir list */
} RESPONSE;

typedef struct _FILE_TYPE{
	char name[MAX_FILE_NAME];
	//chenyao change the timespec
	struct tm modified_datetime;
	int size;
	int type;			/* 0->dir, 1->Regular File */
	//chenyao add the next
	struct _FILE_TYPE *next;
} FILE_TYPE;

typedef struct {
	int   l_req_stat;
	int   l_size;
	int   len;
	char l_addr[INET6_ADDRSTRLEN];
	char l_line[MAX_REQUEST_LEN];
	time_t l_time;
} LOG;
typedef struct{
	char si_name[NAME_MAX];
	char si_address[PATH_MAX];
	char si_port[5];
	char si_rootdir[PATH_MAX];
	char si_cgidir[PATH_MAX];
	char si_logpath[PATH_MAX];
	char si_debugpath[PATH_MAX];
	char si_cflag[5];
	char si_lflag[5];
}SERVERINFO;

extern SERVERINFO server_info;
void ini_conf(SERVERINFO* ser);
char *trim(char*);
void parse_conf(SERVERINFO* info);
extern LOG log_info;

int	check_file(const char *, int);

void start_server();
//struct req_header req_parser(char *);

void ini_request(REQUEST *);
void ini_response(RESPONSE *);
int req_parser(int, REQUEST *);

void ini_cgi(CGI_ENV *);
int cgi_parser(CGI_ENV *, REQUEST *, char *);
int cgi_exec(int *, CGI_ENV *); /* fd should be closed after calling */

void ini_cgi(CGI_ENV *);
int cgi_parser(CGI_ENV *, REQUEST *, char *);
int cgi_exec(int *, CGI_ENV *); /* fd should be closed after calling */

//path ,response,fd
int open_file(char* ,RESPONSE *,int *);
//content , response, path
int read_file(int ,char *, int );
//path , all the files under this absolute path
//chenyao change argu
int disp_dir(char *, FILE_TYPE **);
//get last modify time : path, result
//chenyao change time type
int get_lm_datetime(char *, struct tm *);

char * res_builder(RESPONSE *, REQUEST *, int *, char *);
char * res_datetime_format(struct tm);
char * res_gen_1(RESPONSE *);
char * res_gen_2(RESPONSE *);
char * res_dir_gen(char *, FILE_TYPE *);
int res_send(char *, int, int);

void ini_log(LOG *);
void log_sws(LOG *);

char *print_err(int);
#endif


