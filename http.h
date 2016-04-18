#ifndef _SWS_HTTP_H_
#define _SWS_HTTP_H_

#include <time.h>

#define WKDAY 1
#define WEEKDAY 0
#define TYPE_URI 0
#define TYPE_QUERY 1
#define SENDBUFFSIZE 2000
#define DIRLISTSIZE 8192
#define ITEM_MAX_SIZE 255

/* request */
static void req_line_parser(char *, REQUEST *);
static int ims_parser(char *, REQUEST *);
static int uri_process(char *, REQUEST *);
static int remove_parent_dir(char *);
static int check_special_chars(char *, int);
static char escape_decode(char, char);
static int convert_wkday(char *, int);
static int convert_month(char *);

/* response */
static void resp_header_generator(RESPONSE *, REQUEST *);
static void err_header_generator(RESPONSE *);
static off_t dir_content_len(char *);
static int fts_cmp(const FTSENT **, const FTSENT **);
static int cmp_lexico(const FTSENT *, const FTSENT *);
static void gmt_transfer(time_t *);
static int send_header(int, RESPONSE *, int);
static int write_str(int, char *);
static void send_content(int, RESPONSE *, CGI_ENV *, int);
static void send_dir_content(int, char *);

#endif

