#ifndef _SWS_HTTP_H_
#define _SWS_HTTP_H_

#define WKDAY 1
#define WEEKDAY 0
#define TYPE_URI 0
#define TYPE_QUERY 1
#define REGFILE 1
#define DIRINDEX 0
#define SENDBUFFSIZE 2000
#define DIRLISTSIZE 8192
#define ITEM_MAX_SIZE 255


static int req_line_parser(char *, REQUEST *);
static int ims_parser(char *, REQUEST *);
static int uri_process(char *, REQUEST *);
static int remove_parent_dir(char *);
static int check_special_chars(char *, int);
static char escape_decode(char, char);
static int convert_wkday(char *, int);
static int convert_month(char *);

#endif

