/*
 * http.c		- handle http request and response for `sws` command
 * sws			- a simple web server
 *
 * CS631 final project	- sws
 * Group 		- flag
 * Members 		- Chenyao Wang, cwang60@stevens.edu
 *			- Dongxu Han, dhan7@stevens.edu
 *			- Gong Cheng, gcheng2@stevens.edu
 *			- Maisi Li, mli27@stevens.edu
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <strings.h>

#include "sws.h"
#include "http.h"

/*
 * Initialize REQUEST structure.
 */
void
ini_request(REQUEST *req)
{
	req->method = UNKNOW_METHOD;
	bzero(req->uri, sizeof(req->uri));
	bzero(req->raw_uri, sizeof(req->raw_uri));
	bzero(req->query, sizeof(req->query));
	req->ver = UNKNOW_VER;
	req->is_ims = NOT_SPECIFIED;
	req->is_cgi = NOT_SPECIFIED;
	memset(&(req->if_modified_since), 0, sizeof(req->if_modified_since));
}

/*
 * Initialize RESPONSE structure.
 */
void
ini_response(RESPONSE *resp)
{
	resp->method = UNKNOW_METHOD;
	resp->status = 0;
	memset(&(resp->date), 0, sizeof(resp->date));
	resp->server = SERVER_NAME;
	memset(&(resp->lm_date), 0, sizeof(resp->lm_date));
	resp->content_type = CONTENT_TYPE;
	resp->content_len = 0;
	bzero(resp->content_path,sizeof(resp->content_path));
	resp->ls_type = REGFILE;
}

/*
 * Return STAT_NOT_SPECIFIED on success, or the corresponding status code
 * on error.
 */
int
req_parser(int msgsock, REQUEST *req)
{
	int is_req_line;
	int is_cr;
	char if_modified_since[MAX_REQUEST_LEN];
	char buffer[MAX_REQUEST_LEN];
	char ch;
	int n;
	int i;
	int status_code;
	clock_t timeout;

	is_req_line = 0;
	i = 0;
	is_cr = 0;

	timeout = clock();
REQAGAIN:
	while ((n = read(msgsock, &ch, 1)) > 0) {
		if (i == MAX_REQUEST_LEN - 1) {
			buffer[i] = '\0';
			if (time(&log_info.l_time) == (time_t)(-1))
				return STAT_INTERNAL_SERVER_ERROR;

			strcpy(log_info.l_line, buffer);
			return STAT_BAD_REQUEST;
		}

		if (ch == '\n') { /* LF */
			if (!is_cr) { /* CR isn't read */
				if (time(&log_info.l_time) == (time_t)(-1))
					return STAT_INTERNAL_SERVER_ERROR;

				return STAT_BAD_REQUEST;
			}

			if (i == 1) { /* a new CRLF is received */
				if (is_req_line == 1) /* request end */
					break;
				else { /* skip leading CRLFs */
					bzero(buffer, sizeof(buffer));
					i = 0;
					is_cr = 0;
					continue;
				}
			}

			if (is_req_line == 0) { /* request line */
				buffer[i] = '\n';
				buffer[i + 1] = '\0';
				strncpy(log_info.l_line, buffer, i - 1);
				status_code = req_line_parser(buffer, req);

				if (status_code != STAT_NOT_SPECIFIED) {
					if (time(&log_info.l_time) ==
							(time_t)(-1))
					return STAT_INTERNAL_SERVER_ERROR;
				return status_code;
				}
				is_req_line = 1;
				bzero(buffer,sizeof(buffer));
				i = 0;
			} else if (is_cr) {
				buffer[i] = '\n';
				buffer[i + 1] = '\0';
				if (strncmp(buffer, "If-Modified-Since:", 18)
					== 0) {
					strcpy(if_modified_since, buffer);
				}
				bzero(buffer,sizeof(buffer));
				i = 0;
			}
		} else if (ch == '\r') {
			is_cr = 1;
			buffer[i] = ch;
			i++;
		} else {
			is_cr = 0;
			buffer[i] = ch;
			i++;
		}
	}

	if (n < 0) {
		if (errno == EBLOCK) {
			if(((clock() - timeout) / CLOCKS_PER_SEC) > 60) {
				if (time(&log_info.l_time) == (time_t)(-1))
					return STAT_INTERNAL_SERVER_ERROR;

				return STAT_REQUEST_TIMEOUT;
			} else
				goto REQAGAIN;
		}

		if (time(&log_info.l_time) == (time_t)(-1))
			return STAT_INTERNAL_SERVER_ERROR;

		return STAT_INTERNAL_SERVER_ERROR;

	}


	if ((n = ims_parser(if_modified_since, req)) == 0)
		req->is_ims = SPECIFIED;
	else if (n == -1)
		req->is_ims = NOT_SPECIFIED;
	else
		status_code = STAT_INTERNAL_SERVER_ERROR;

	if (time(&log_info.l_time) == (time_t)(-1))
		return STAT_INTERNAL_SERVER_ERROR;

	return status_code;
}

/*
 * Parse the request line.
 * Return STAT_NOT_SPECIFIED on success, or the corresponding status code
 * on error.
 */
static int
req_line_parser(char *req_line, REQUEST *req)
{
	char method[7];		/* HEAD or GET */
	char sp1, sp2;		/* ascii space (32) */
	char garbage[2];	/* garbage before CRLF */
	char uri[URI_BUFSIZE];
	char version[9];	/* HTTP/1.0 or HTTP/0.9 */
	int n;
	int status_code;
	int ret_val;

	status_code = STAT_NOT_SPECIFIED;

	if ((n = sscanf(req_line,
			/*Method   SP URI   SP HTTP-Version CRLF */
			"%6[A-Z]%c %4095s%c %8[HTP/.019] %1[^\r\n]",
			method, &sp1, uri, &sp2, version, garbage)) == EOF) {
		if (errno != 0)
			return STAT_INTERNAL_SERVER_ERROR;
		else
			return STAT_BAD_REQUEST;
	}
	/* garbage should not match any character, so n equals 5 instead of 6 */
	else if (n == 0 || n == 6) {
		return STAT_BAD_REQUEST;
	} else {
		if (n >= 1) {
			if (strcmp(method, "GET") == 0) {
				req->method = GET;
			}
			else if (strcmp(method, "HEAD") == 0)
				req->method = HEAD;
			else {
				req->method = UNKNOW_METHOD;
				if ((strcmp(method, "POST") == 0) ||
					(strcmp(method, "PUT") == 0) ||
					(strcmp(method, "DELETE") == 0) ||
					(strcmp(method, "LINK") == 0) ||
					(strcmp(method, "UNLINK") == 0)) {
					status_code = STAT_NOT_IMPLEMENTED;
				}
				else {
					return STAT_BAD_REQUEST;
				}
			}
		}

		if (n >= 2)
			if (sp1 != SP) {
				return STAT_BAD_REQUEST;
			}

		if (n >= 3) {
            strcpy(req->raw_uri, uri);
			ret_val = uri_process(uri, req);
			if (ret_val == -1) {
				return STAT_BAD_REQUEST;
			} else if (ret_val == -2) {
				return STAT_FORBIDDEN;
			}
		}

		if (n >= 4)
			if (sp2 != SP) {
				return STAT_BAD_REQUEST;
			}

		if (n == 5) {
			if (strcmp(version, "HTTP/1.0") == 0)
				req->ver = HTTP10;
			else if (strcmp(version, "HTTP/0.9") == 0)
				req->ver = HTTP09;
			else {
				req->ver = UNKNOW_VER;
				return STAT_BAD_REQUEST;
			}

			/* end up with CRLF */
			if (strcmp(req_line + strlen(req_line) - 2, CRLF)
				== 0) {
				return status_code;
			} else {
				return STAT_BAD_REQUEST;
			}
		}

		return  STAT_BAD_REQUEST;
	}
}

/*
 * generate the http header string of response message
 *
 * return null if internally failed, otherwise return
 * string contains http header with certain status code
 */

char *
res_builder(RESPONSE * resp, REQUEST * req, int * fd, char * c_ip)
{
	char * header;
	int status;
	FILE_TYPE * filelist = NULL;
	struct tm * p, * lm_datetime = NULL;
	time_t timep;
	CGI_ENV cgi;
	int cgi_fd;
	char * ptr1, * ptr2, *ptr3;	




    if ((header = malloc(1024*sizeof(char))) == NULL){
        resp->status = STAT_INTERNAL_SERVER_ERROR;
        return NULL;
    }
	header[0] = '\0';
	if (req->is_ims == SPECIFIED) {
		if (get_lm_datetime(req->uri, lm_datetime) != 0){
			resp->status = STAT_INTERNAL_SERVER_ERROR;
		}
		else{
			resp->lm_date = *lm_datetime;
            if (mktime(lm_datetime) < mktime(&req->if_modified_since))
                resp->status = STAT_NOT_MODIFIED;
		}
	}

	if ((resp->status == STAT_NOT_SPECIFIED) && (req->is_cgi != SPECIFIED)){
		if ((status = open_file(req->uri, resp, fd)) != STAT_OK) {
			if (STAT_DIR_LIST == status) {
				resp->ls_type = DIRINDEX;
				*fd = 0;
				if ((status = disp_dir(req->uri, &filelist)) != 0){
					//perror("Directory indexing error");
					resp->status = STAT_FORBIDDEN;
				}
                else
                    resp->status = STAT_OK;
			}
			else
                resp->status = status;
		}
		else
            resp->status = STAT_OK;
	}

	time(&timep);
	p = gmtime(&timep);

	if (req->ver == HTTP09){
		if (resp->status != STAT_OK){
			if ((ptr2 = print_err(resp->status)) == NULL)
				resp->status = STAT_INTERNAL_SERVER_ERROR;
			else{
				*fd = 0;
				strcat(header, ptr2);
			}
		}
		else if (resp->ls_type == DIRINDEX){
			if ((ptr3 = res_dir_gen((char *)&req->raw_uri, filelist)) == NULL){
				return NULL;
			}
			else
				strcat(header, ptr3);
		}

		return header;
	}

	resp->method = req->method;
	resp->date = *p;
	resp->server = SERVER_NAME;
	if (resp->method == HEAD)
        *fd = 0;

	if ( (ptr1 = res_gen_1(resp)) == NULL){
		return NULL;
	}

    if (resp->status >= 304){
        strcat(header, ptr1);
        *fd = 0;
		if ((ptr2 = res_gen_2(resp)) == NULL){
            resp->status = STAT_INTERNAL_SERVER_ERROR;
            return NULL;
		}
		else
			strcat(header, ptr2);
        if ((ptr3 = print_err(resp->status)) == NULL)
            resp->status = STAT_INTERNAL_SERVER_ERROR;
        else
            strcat(header, ptr3);
        return header;
    }

	if ((resp->ls_type == DIRINDEX)){
            if ((ptr3 = res_dir_gen((char *)&req->raw_uri, filelist)) == NULL){
                    return NULL;
            }
			resp->content_len = strlen(ptr3);
	}

	if (req->is_cgi != SPECIFIED){
		if ((ptr2 = res_gen_2(resp)) == NULL){
            resp->status = STAT_INTERNAL_SERVER_ERROR;
            return NULL;
		}

		else{
            strcat(header, ptr1);
            strcat(header, ptr2);
            if ((resp->ls_type == DIRINDEX) && (resp->method != HEAD)){
                strcat(header, ptr3);
            }
            return header;
		}
	}
	else{
  	        ini_cgi(&cgi);
   	     if ((status = cgi_parser(&cgi, req, c_ip)) != STAT_NOT_SPECIFIED){
            //perror("CGI parse error");
       		     resp->status = status;
	        }
     		if ((status = cgi_exec(&cgi_fd, &cgi)) != STAT_NOT_SPECIFIED){
            //perror("CGI exec error");
       		     resp->status = status;
	        }
		if (req->method != HEAD)
 		     *fd = cgi_fd;
	        else
  	     	     *fd = 0;

		if (resp->status == STAT_NOT_SPECIFIED)
			resp->status = STAT_OK;
		if ( (ptr1 = res_gen_1(resp)) == NULL){
			//perror("Response generate 1 error");
			return NULL;
		}

		if (resp->status >= 304){
			strcat(header, ptr1);
			strcat(header, CRLF);
			*fd = 0;
			if ((ptr2 = print_err(resp->status)) == NULL)
				resp->status = STAT_INTERNAL_SERVER_ERROR;
			else
				strcat(header, ptr2);
			return header;
		}

	        strcat(header, ptr1);
		return header;
	}
}

/*
 * Print html code for display of directory index
 */
char *
res_dir_gen(char * uri, FILE_TYPE * f_list)
{
    char * ret_val;
    char tmp[ITEM_MAX_SIZE];
    char tmp1[ITEM_MAX_SIZE];
    char * tmp_ptr;

    if ((ret_val = malloc(DIRLISTSIZE*sizeof(char))) == NULL){
        return NULL;
    }
    memset(ret_val, 0, DIRLISTSIZE);

    memset(tmp, 0, ITEM_MAX_SIZE);
    strcat(ret_val, "<!DOCTYPE html>\r\n<html>");
    sprintf(tmp, "<head><meta charset=\"utf-8\"><title>Index of %s</title></head>", uri);
    strcat(ret_val, tmp);
    memset(tmp, 0, ITEM_MAX_SIZE);
    sprintf(tmp, "<body><h1>Index of %s</h1><hr><table>", uri);
    strcat(ret_val, tmp);

    memset(tmp, 0, ITEM_MAX_SIZE);
    sprintf(tmp, "<tr><td><a href=\"%s/\">%s/</a></td></tr><br />", "..", "..");
    strcat(ret_val, tmp);
    while (f_list != NULL){
        memset(tmp, 0, ITEM_MAX_SIZE);
        memset(tmp1, 0, ITEM_MAX_SIZE);
        if (f_list->type == REGFILE)
            sprintf(tmp, "<tr><td><a href=\"%-s\">%s</a></td>", 
				f_list->name, f_list->name);
        else
            sprintf(tmp, "<tr><td><a href=\"%s/\">%s/</a></td>", 
				f_list->name, f_list->name);
        sprintf(tmp1, "%s", tmp);
        strcat(ret_val, tmp1);
        tmp_ptr = res_datetime_format(f_list->modified_datetime);
        sprintf(tmp, "<td>%s</td>", tmp_ptr);
        strcat(ret_val, tmp);
        sprintf(tmp, "<td align=\"right\">%d</td></tr>\n", f_list->size);
        strcat(ret_val, tmp);
        f_list = f_list->next;
    }

    strcat(ret_val, "</table><hr></body></html>\0");
    return ret_val;
}

/*
 * Generate first part of response header
 *
 * Used for cgi handle, general file return and directory listing
 */
char *
res_gen_1(RESPONSE * resp)
{
	char status_line[20];
	char status_code[10];
	char * dt;
	char tmp[ITEM_MAX_SIZE];
	char * ret;


    if ((ret = malloc(ITEM_MAX_SIZE*sizeof(char))) == NULL){
        resp->status = STAT_INTERNAL_SERVER_ERROR;
        return NULL;
    }
	memset(tmp, 0, ITEM_MAX_SIZE);
	memset(ret, 0, ITEM_MAX_SIZE);
	memset(status_line, 0, 20);
	memset(status_line, 0, 10);

	strcpy(status_line, "HTTP/1.0 ");
	sprintf(status_code, "%d%s", resp->status, CRLF);
	strcat(status_line, status_code);
	strcat(ret, status_line);

	dt = res_datetime_format(resp->date);
	sprintf(tmp, "Date: %s%s",dt, CRLF);
	strcat(ret, tmp);
	memset(tmp, 0, ITEM_MAX_SIZE);

	sprintf(tmp, "Server: %s%s",resp->server, CRLF);
	strcat(ret, tmp);
	memset(tmp, 0, ITEM_MAX_SIZE);

    if (resp->lm_date.tm_year != 0){
        dt = res_datetime_format(resp->lm_date);
        sprintf(tmp, "Last-Modified: %s%s",dt, CRLF);
        strcat(ret, tmp);
        memset(tmp, 0, ITEM_MAX_SIZE);
    }

	return ret;
}

/*
 * Generate second part of response header
 *
 * Used for general file return only
 */
char *
res_gen_2(RESPONSE * resp)
{
	char tmp[ITEM_MAX_SIZE];
	char * ret;

    if ((ret = malloc(ITEM_MAX_SIZE*sizeof(char))) == NULL){
        resp->status = STAT_INTERNAL_SERVER_ERROR;
        return NULL;
    }
	memset(tmp, 0, ITEM_MAX_SIZE);

	sprintf(tmp, "Content-Type: %s%s",resp->content_type, CRLF);
	strcat(ret, tmp);
	memset(tmp, 0, ITEM_MAX_SIZE);

	sprintf(tmp, "Content-Length: %d%s",(int)resp->content_len, CRLF);
	strcat(ret, tmp);
	memset(tmp, 0, ITEM_MAX_SIZE);

	strcat(ret, CRLF);

	return ret;

}

/*
 * Generate the datetime format used for http header
 *
 * Return the string contains formatted datetime in GMT
 */
char *
res_datetime_format(struct tm lm)
{
	char *wday[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
	char *month[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
					 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
	char *ret_val = malloc(30*sizeof(char));
	sprintf(ret_val, "%s, %d %s %d:%d:%d GMT", wday[lm.tm_wday],
			lm.tm_mday, month[lm.tm_mon], lm.tm_hour,
			lm.tm_min, lm.tm_sec);
	return ret_val;
}

/*
 * Send the disposed data back to browser
 *
 * Read entity body directly from file describer so that
 * can handle big file by sending its small chunk
 * each time
 */
int
res_send(char * ret, int fd, int msgsock)
{
	char buff[SENDBUFFSIZE+1];
	int offset = 0, inc_val;
	int n, head_len, total_len;

    head_len = strlen(ret);
	if (head_len != 0){
		do{
            if (offset + SENDBUFFSIZE > head_len)
                inc_val = head_len - offset;
            else
                inc_val = SENDBUFFSIZE;
			if ((n = write(msgsock, ret+offset, inc_val)) <= 0){
				//perror("header response send error");
				return 1;
			}
			if (n != SENDBUFFSIZE)
                break;
			offset += SENDBUFFSIZE;
		} while (offset-SENDBUFFSIZE <= head_len);
	}
    total_len = head_len;
    if (fd != 0){
        while ((n = read(fd, buff, SENDBUFFSIZE)) > 0){
            if (write(msgsock, buff, n) <= 0){
                //perror("content response send error");
                (void)close(fd);
                return 1;
            }
            total_len += n;
        }
        (void)close(fd);
    }
    log_info.l_size = total_len;

    return 0;
}


/*
 * Parse the If-Modified-Since header.
 *
 * Return 0 if If-Modified-Since is specified, return -1 if If-Modified-Since
 * not specfied, return -2 if internal server error happened.
 */
static int
ims_parser(char *if_modified_since, REQUEST *req)
{
	char wkday[10];
	char month[4];
	char garbage[2];
	int weekday;
	int day;
	int mon;
	int year;
	int hour;
	int minute;
	int second;
	int n;
	int len;
	char sp1, sp2, sp3, sp4, sp5; /* acsii space (32) */

	if (req->ver == HTTP09 || strcmp(if_modified_since, "") == 0) {
		return -1;
	}

	len = strlen(if_modified_since);
	/*
	 * garbage should not match any character, so n should be 1 less
	 * than the number of arguments.
	 */
	if ((n = sscanf(if_modified_since, "If-Modified-Since: %3[A-Za-z],%c"
			" %2d%c %3[A-Za-z]%c %4d%c %2d:%2d:%2d%c GMT %1[\r\n]",
			wkday, &sp1, &day, &sp2, month, &sp3, &year, &sp4,
			&hour, &minute, &second, &sp5, garbage)) == EOF) {
		if (errno != 0)
			return -2;
		return -1;
	} else if (n == 12) { /* rfc1123-date */
		if (sp1 != SP || sp2 != SP || sp3 != SP || sp4 != SP ||
			sp5 != SP || strcmp(if_modified_since + len - 2,
			CRLF) != 0) {
			return -1;
		}

		if ((weekday = convert_wkday(wkday, WKDAY)) == -1 ||
		(mon = convert_month(month)) == -1) {
			return -1;
		}

		req->if_modified_since.tm_sec = second;
		req->if_modified_since.tm_min = minute;
		req->if_modified_since.tm_hour = hour;
		req->if_modified_since.tm_mday = day;
		req->if_modified_since.tm_mon = mon;
		req->if_modified_since.tm_year = year - 1900;
		req->if_modified_since.tm_wday = weekday;

		/*
		 * mktime(3) modifies the tm structure, which helps us
		 * check the accuracy of the input date
		 */
		if (mktime(&(req->if_modified_since)) == (time_t)(-1)) {
			return -2;
		}

		if (req->if_modified_since.tm_sec == second &&
		    req->if_modified_since.tm_min == minute &&
		    req->if_modified_since.tm_hour == hour &&
		    req->if_modified_since.tm_mday == day &&
		    req->if_modified_since.tm_mon == mon &&
		    req->if_modified_since.tm_year == year - 1900 &&
		    req->if_modified_since.tm_wday == weekday) {
			return 0;
		} else {
			return -1;
		}
	} else if ((n = sscanf(if_modified_since, "If-Modified-Since: "
				"%9[A-Za-z],%c %2d-%3[A-Za-z]-%2d%c "
				"%2d:%2d:%2d%c GMT %1[^\r\n]", wkday, &sp1,
				&day, month, &year, &sp2, &hour, &minute,
				&second, &sp3, garbage))
				== EOF) {
		if (errno != 0)
			return -2;
		return -1;
	} else if (n == 10) { /* rfc850-date */
		if (sp1 != SP || sp2 != SP || sp3 != SP ||
			strcmp(if_modified_since + len - 2, CRLF) != 0) {
			return -1;
		}

		if ((weekday = convert_wkday(wkday, WEEKDAY)) == -1 ||
		(mon = convert_month(month)) == -1) {
			return -1;
		}

		req->if_modified_since.tm_sec = second;
		req->if_modified_since.tm_min = minute;
		req->if_modified_since.tm_hour = hour;
		req->if_modified_since.tm_mday = day;
		req->if_modified_since.tm_mon = mon;
		req->if_modified_since.tm_year = year;
		req->if_modified_since.tm_wday = weekday;

		/*
		 * mktime(3) modifies the tm structure, which helps us
		 * check the accuracy of the input date
		 */
		if (mktime(&(req->if_modified_since)) == (time_t)(-1)) {
			return -2;
		}

		if (req->if_modified_since.tm_sec == second &&
		    req->if_modified_since.tm_min == minute &&
		    req->if_modified_since.tm_hour == hour &&
		    req->if_modified_since.tm_mday == day &&
		    req->if_modified_since.tm_mon == mon &&
		    req->if_modified_since.tm_year == year &&
		    req->if_modified_since.tm_wday == weekday) {
			return 0;
		} else {
			return -1;
		}
	} else if ((n = sscanf(if_modified_since, "If-Modified-Since: "
				"%3[A-Za-z]%c %3[A-Za-z]%c %2d%c "
				"%2d:%2d:%2d%c %4d %1[^\r\n]", wkday, &sp1,
				month, &sp2, &day, &sp3, &hour, &minute,
				&second, &sp4, &year, garbage)) == EOF) {
		if (errno != 0)
			return -2;
		return -1;
	} else if (n == 11) { /* asctime() format */
		if (sp1 != SP || sp2 != SP || sp3 != SP || sp4 != SP ||
			strcmp(if_modified_since + len - 2, CRLF) != 0) {
			return -1;
		}

		if ((weekday = convert_wkday(wkday, WKDAY)) == -1 ||
		(mon = convert_month(month)) == -1) {
			return -1;
		}

		req->if_modified_since.tm_sec = second;
		req->if_modified_since.tm_min = minute;
		req->if_modified_since.tm_hour = hour;
		req->if_modified_since.tm_mday = day;
		req->if_modified_since.tm_mon = mon;
		req->if_modified_since.tm_year = year - 1900;
		req->if_modified_since.tm_wday = weekday;

		/*
		 * mktime(3) modifies the tm structure, which helps us
		 * check the accuracy of the input date
		 */
		if (mktime(&(req->if_modified_since)) == (time_t)(-1)) {
			return -2;
		}

		if (req->if_modified_since.tm_sec == second &&
		    req->if_modified_since.tm_min == minute &&
		    req->if_modified_since.tm_hour == hour &&
		    req->if_modified_since.tm_mday == day &&
		    req->if_modified_since.tm_mon == mon &&
		    req->if_modified_since.tm_year == year - 1900 &&
		    req->if_modified_since.tm_wday == weekday) {
			return 0;
		} else {
			return -1;
		}

	} else
		return -1;
}

/*
 * Process the request URI, check the format, ignore both './' and '../'.
 * Return 0 on success, -1 on bad request, -2 on forbidden(cgi not allowed).
 */
static int
uri_process(char *uri, REQUEST *req)
{
	int i;
	int query;
	int len;

	/* split URI and query */
	if ((query = remove_parent_dir(uri)) != -1) {
		strcpy(req->query, uri + query + 1);
		uri[query] = '\0';
		strcpy(req->uri, uri);
	} else
		strcpy(req->uri, uri);

	if (check_special_chars(req->uri, TYPE_URI) == -1 ||
		check_special_chars(req->query, TYPE_QUERY) == -1)
		return -1;

	strcpy(uri, req->uri);

	if (uri[0] == '/') {
		/* cgi execution */
		if (strncmp(uri, "/cgi-bin/", 9) == 0) {
			if (!cflag)
				return -2;
			snprintf(req->uri, URI_BUFSIZE, "%s%s", cgi_dir,
					uri + 9);
			req->is_cgi = SPECIFIED;
		} else if (uri[1] == '~') {	/* user directories */
			len = strlen(uri);
			for (i = 2; i < len; i++)
				if (uri[i] == '/')
					break;
			if (i < len) {
				uri[i] = '\0';
				snprintf(req->uri, URI_BUFSIZE,
					"/home/%s/sws/%s", uri + 2,
					uri + i + 1);
			} else
				snprintf(req->uri, URI_BUFSIZE, "/home/%s/sws/",
					uri + 2);
		} else	/* regular directory or file */
			snprintf(req->uri, URI_BUFSIZE, "%s%s", doc_root,
					uri + 1);
		return 0;
	} else
		return -1;
}

/*
 * Remove all './'s and '../'s.
 * Return the location where first '?' is found, otherwise -1.
 */
static int
remove_parent_dir(char * uri)
{
	int i, j;
	int len;
	int query;

	len = strlen(uri);
	query = -1;
	for (i = 0, j = 0; i < len;) {
		if (uri[i] == '?') {
			query = j;
			for (; i < len; i++, j++)
				uri[j] = uri[i];
			break;
		}
		uri[j] = uri[i];
		j++;

		if (uri[i] == '/') {
			while (i < len) {
				if (strncmp(uri + i + 1, "./", 2) == 0)
					i += 2;
				else if (strncmp(uri + i + 1, "../", 3) == 0)
					i += 3;
				else
					break;
			}
		}

		i++;
	}
	uri[j] = '\0';
	return query;
}

static int
check_special_chars(char *uri, int type)
{
	int i, j;
	int len;
	char c;

	len = strlen(uri);
	for (i = 0, j = 0; i < len; i++, j++) {
		/* unsafe characters */
		c = uri[i];

		if (uri[i] <= 32 || uri[i] == '\"' || uri[i] == '#' ||
			uri[i] == '<' || uri[i] == '>')
			return -1;

		if (type == TYPE_URI) {
			if (uri[i] == '%') {
				if (i + 2 >= len)
					return -1;
				if ((c = escape_decode(uri[i + 1],
							uri[i + 2])) == 0)
					return -1;
				i += 2;
			}

			/* reserved characters */
			if (uri[i] == ';' || uri[i] == '?')
				return -1;
		}
		uri[j] = c;
	}
	uri[j] = '\0';
	return 0;
}

/* decode the encoded unsafe characters */
static char
escape_decode(char hex1, char hex2)
{
	if (hex1 == '2' && hex2 == '0')
		return ' ';
	else if (hex1 == '2' && hex2 == '2')
		return '\"';
	else if (hex1 == '2' && hex2 == '3')
		return '#';
	else if (hex1 == '2' && hex2 == '5')
		return '%';
	else if (hex1 == '3' && hex2 == 'C')
		return '<';
	else if (hex1 == '3' && hex2 == 'E')
		return '>';
	return 0;
}

/*
 * Return -1 on error.
 * Return 0-6 on success, begin with "Sun" or "Sunday".
 */
static int
convert_wkday(char *wkday, int flag)
{
	if (flag == WKDAY) {
		if (strcmp(wkday, "Sun") == 0)
			return 0;
		else if (strcmp(wkday, "Mon") == 0)
			return 1;
		else if (strcmp(wkday, "Tue") == 0)
			return 2;
		else if (strcmp(wkday, "Wed") == 0)
			return 3;
		else if (strcmp(wkday, "Thu") == 0)
			return 4;
		else if (strcmp(wkday, "Fri") == 0)
			return 5;
		else if (strcmp(wkday, "Sat") == 0)
			return 6;
		else
			return -1;
	} else if (flag == WEEKDAY) {
		if (strcmp(wkday, "Sunday") == 0)
			return 0;
		else if (strcmp(wkday, "Monday") == 0)
			return 1;
		else if (strcmp(wkday, "Tuesday") == 0)
			return 2;
		else if (strcmp(wkday, "Wednesday") == 0)
			return 3;
		else if (strcmp(wkday, "Thursday") == 0)
			return 4;
		else if (strcmp(wkday, "Friday") == 0)
			return 5;
		else if (strcmp(wkday, "Saturday") == 0)
			return 6;
		else
			return -1;
	} else
		return -1;
}

/*
 * Return -1 on error.
 * Return 0-11 on success, begin with "Jan".
 */
static int
convert_month(char *month)
{
	if (strcmp(month, "Jan") == 0)
		return 0;
	else if (strcmp(month, "Feb") == 0)
		return 1;
	else if (strcmp(month, "Mar") == 0)
		return 2;
	else if (strcmp(month, "Apr") == 0)
		return 3;
	else if (strcmp(month, "May") == 0)
		return 4;
	else if (strcmp(month, "Jun") == 0)
		return 5;
	else if (strcmp(month, "Jul") == 0)
		return 6;
	else if (strcmp(month, "Aug") == 0)
		return 7;
	else if (strcmp(month, "Sep") == 0)
		return 8;
	else if (strcmp(month, "Oct") == 0)
		return 9;
	else if (strcmp(month, "Nov") == 0)
		return 10;
	else if (strcmp(month, "Dec") == 0)
		return 11;
	else
		return -1;
}


