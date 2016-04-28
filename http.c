#include <sys/stat.h>

#include <errno.h>
#include <fts.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>

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
	req->status = STAT_NOT_SPECIFIED;
	bzero(req->uri, sizeof(req->uri));
	bzero(req->raw_uri, sizeof(req->raw_uri));
	bzero(req->query, sizeof(req->query));
	req->ver = UNKNOW_VER;
	req->is_ims = NOT_SPECIFIED;
	req->is_cgi = NOT_SPECIFIED;
	memset(&(req->if_modified_since), 0, sizeof(req->if_modified_since));
	req->if_modified_since.tm_isdst = -1;
}

/*
 * Initialize RESPONSE structure.
 */
void
ini_response(RESPONSE *resp)
{
	resp->method = UNKNOW_METHOD;
	resp->status = STAT_NOT_SPECIFIED;
	resp->curr_date = (time_t)(-1);
	resp->server = SERVER_NAME;
	resp->lm_date = (time_t)(-1);
	resp->content_type = CONTENT_TYPE;
	resp->content_len = 0;
	bzero(resp->content_path, sizeof(resp->content_path));
	bzero(resp->raw_uri, sizeof(resp->raw_uri));
	resp->ls_type = FILETYPE;
}

/*
 * Parse request.
 */
void
req_parser(int msgsock, REQUEST *req, LOG *log_info)
{
	int is_req_line;
	int is_cr;
	char if_modified_since[MAX_REQUEST_LEN];
	char buffer[MAX_REQUEST_LEN];
	char ch;
	int n;
	int i;
	clock_t timeout;

	is_req_line = 0;
	i = 0;
	is_cr = 0;

	bzero(if_modified_since, sizeof(if_modified_since));
	bzero(buffer, sizeof(buffer));

	timeout = clock();
REQAGAIN:
	while ((n = read(msgsock, &ch, 1)) > 0) {
		if (i == MAX_REQUEST_LEN - 1) { /* request too long */
			buffer[i] = '\0';

			strcpy(log_info->l_line, buffer);

			if (time(&log_info->l_time) == (time_t)(-1)) {
				req->status = STAT_INTERNAL_SERVER_ERROR;
				return;
			}

			gmt_transfer(&log_info->l_time);
			req->status = STAT_BAD_REQUEST;
			return;
		}

		if (ch == '\n') { /* LF */
			if (!is_cr) { /* CR isn't read */
				if (time(&log_info->l_time) == (time_t)(-1)) {
					req->status = STAT_INTERNAL_SERVER_ERROR;
					return;
				}

				gmt_transfer(&log_info->l_time);
				req->status = STAT_BAD_REQUEST;
				return;
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
				/* remove trailing CRLF */
				strncpy(log_info->l_line, buffer, i - 1);
				log_info->l_line[i] = '\0';

				req_line_parser(buffer, req);

				if (req->status != STAT_NOT_SPECIFIED) {
					if (time(&log_info->l_time) ==
							(time_t)(-1)) {
						req->status = STAT_INTERNAL_SERVER_ERROR;
					}

					gmt_transfer(&log_info->l_time);	
					return;
				}

				is_req_line = 1;
				bzero(buffer,sizeof(buffer));
				i = 0;
			} else {
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
				if (time(&log_info->l_time) == (time_t)(-1)) {
					req->status = STAT_INTERNAL_SERVER_ERROR;
					return;
				}

				gmt_transfer(&log_info->l_time);
				req->status = STAT_REQUEST_TIMEOUT;
				return;
			} else
				goto REQAGAIN;
		}

		if (time(&log_info->l_time) == (time_t)(-1)) {
			req->status = STAT_INTERNAL_SERVER_ERROR;
			return;
		}

		gmt_transfer(&log_info->l_time);
		req->status = STAT_INTERNAL_SERVER_ERROR;
		return;
	}


	if ((n = ims_parser(if_modified_since, req)) == 0)
		req->is_ims = SPECIFIED;
	else if (n == -1)
		req->is_ims = NOT_SPECIFIED;
	else
		req->status = STAT_INTERNAL_SERVER_ERROR;

	if (time(&log_info->l_time) == (time_t)(-1)) {
		req->status = STAT_INTERNAL_SERVER_ERROR;
		return;
	}

	gmt_transfer(&log_info->l_time);
}

void
response(int msgsock, RESPONSE *resp, REQUEST *req, CGI_ENV *cgi, 
		LOG *log_info) {
	int flag;

	resp_header_generator(resp, req);

	flag = send_header(msgsock, resp, req->is_cgi);

	log_info->status = resp->status;
	log_info->size = resp->content_len;

	if (flag == -1)
		return;

	send_content(msgsock, resp, cgi, req->is_cgi);

}

/*
 * Parse the request line.
 */
static void
req_line_parser(char *req_line, REQUEST *req)
{
	char method[7];		/* HEAD or GET */
	char sp1, sp2;		/* ascii space (32) */
	char garbage[2];	/* garbage before CRLF */
	char uri[URI_BUFSIZE];
	char version[9];	/* HTTP/1.0 or HTTP/0.9 */
	int n;
	int ret_val;

	bzero(method, sizeof(method));
	bzero(garbage, sizeof(garbage));
	bzero(uri, sizeof(uri));
	bzero(version, sizeof(version));
	
	if ((n = sscanf(req_line,
			/*Method   SP URI   SP HTTP-Version CRLF */
			"%6[A-Z]%c %4095s%c %8[HTP/.019] %1[^\r\n]",
			method, &sp1, uri, &sp2, version, garbage)) == EOF) {
		if (errno != 0)
			req->status = STAT_INTERNAL_SERVER_ERROR;
		else
			req->status = STAT_BAD_REQUEST;
		return;
	}
	/* garbage should not match any character, so n equals 5 instead of 6 */
	else if (n == 0 || n == 6) {
		req->status = STAT_BAD_REQUEST;
		return;
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
					req->status = STAT_NOT_IMPLEMENTED;
				}
				else {
					req->status = STAT_BAD_REQUEST;
					return;
				}
			}
		}

		if (n >= 2)
			if (sp1 != SP) {
				req->status = STAT_BAD_REQUEST;
				return;
			}

		if (n >= 3) {
            strcpy(req->raw_uri, uri);
			ret_val = uri_process(uri, req);
			if (ret_val == -1) {
				req->status = STAT_BAD_REQUEST;
				return;
			} else if (ret_val == -2) {
				req->status = STAT_FORBIDDEN;
				return;
			}
		}

		if (n >= 4)
			if (sp2 != SP) {
				req->status = STAT_BAD_REQUEST;
				return;
			}

		if (n == 5) {
			if (strcmp(version, "HTTP/1.0") == 0)
				req->ver = HTTP10;
			else if (strcmp(version, "HTTP/0.9") == 0)
				req->ver = HTTP09;
			else {
				req->ver = UNKNOW_VER;
				req->status = STAT_BAD_REQUEST;
				return;
			}

			/* end up with CRLF */
			if (strcmp(req_line + strlen(req_line) - 2, CRLF)
				== 0) {
				return;
			} else {
				req->status = STAT_BAD_REQUEST;
				return;
			}
		}

		req->status = STAT_BAD_REQUEST;
	}
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

	bzero(wkday, sizeof(wkday));
	bzero(month, sizeof(month));
	bzero(garbage, sizeof(garbage));
	
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

static void
resp_header_generator(RESPONSE *resp, REQUEST *req) {
	struct stat st, index_st;
	int len;
	char index_path[PATH_MAX];
	time_t ims;

	bzero(index_path, sizeof(index_path));

	resp->method = req->method;
	resp->status = req->status;
	strcpy(resp->raw_uri, req->raw_uri);
	time(&resp->curr_date);
	gmt_transfer(&resp->curr_date);

	if (resp->status != STAT_NOT_SPECIFIED) {
		err_header_generator(resp);
		return;
	}

	resp->status = STAT_OK;

	strncpy(resp->content_path, req->uri, PATH_MAX - 1);
	resp->content_path[PATH_MAX - 1] = '\0';

	/* not exist */
	if (access(resp->content_path, F_OK) != 0) {
		resp->status = STAT_NOT_FOUND;
		err_header_generator(resp);
		return;
	}

	if (stat(resp->content_path, &st) < 0) {
		resp->status = STAT_INTERNAL_SERVER_ERROR;
		err_header_generator(resp);
		return;
	}

	/* not a regular file or a directory */
	if (!S_ISREG(st.st_mode) && !S_ISDIR(st.st_mode)) {
		resp->status = STAT_FORBIDDEN;
		err_header_generator(resp);
		return;
	}

	/* file */
	if (S_ISREG(st.st_mode)) {
		if (req->is_cgi == SPECIFIED) {
			if (access(resp->content_path, X_OK) != 0) {
				resp->status = STAT_FORBIDDEN;
				err_header_generator(resp);
				return;
			}
			return;
		}

		/* read permission denied */
		if (access(resp->content_path, R_OK) != 0) {
			resp->status = STAT_FORBIDDEN;
			err_header_generator(resp);
			return;
		}

		resp->lm_date = st.st_mtime;
		gmt_transfer(&resp->lm_date);
		resp->content_len = (uintmax_t)st.st_size;

		/* If-Modified-Since */
		if (req->method == GET && req->is_ims == SPECIFIED) {
			ims = mktime(&req->if_modified_since);
			if (ims < resp->curr_date && resp->lm_date < ims) {
				resp->status = STAT_NOT_MODIFIED;
				err_header_generator(resp);
				return;
			}
		}

		len = strlen(resp->content_path);
		if (len > 3 && strcmp(resp->content_path + len - 3,
								".js") == 0) {
			resp->content_type = CONTENT_TYPE_JS;
		} else if (len > 4 && strcmp(resp->content_path + len - 4,
									".css") == 0) {
			resp->content_type = CONTENT_TYPE_CSS;
		} else if (len > 5 && strcmp(resp->content_path + len - 5,
									".html") == 0) {
			resp->content_type = CONTENT_TYPE_HTML;
		} else {
			resp->content_type = CONTENT_TYPE;
		}
		return;
	}

	/* directory */
	if (req->is_cgi == SPECIFIED) {
		resp->status = STAT_FORBIDDEN;
		err_header_generator(resp);
		return;
	}

	 /* read permission denied */
	if (access(resp->content_path, R_OK) != 0) {
		resp->status = STAT_FORBIDDEN;
		err_header_generator(resp);
		return;
	}

	len = strlen(resp->content_path);

	/* redirect directory without trailing slash */
	if (len > 0 && resp->content_path[len - 1] != '/') {
		len = strlen(resp->raw_uri);
		if (len >= URI_BUFSIZE - 1) {
			resp->status = STAT_BAD_REQUEST;
			err_header_generator(resp);
			return;
		}
		strcat(resp->raw_uri, "/");
		resp->status = STAT_MOVED_PERMANENTLY;
		err_header_generator(resp);
		resp->content_len += (uintmax_t)(len + 1);
		return;
	}

	if (len > 0 && len < PATH_MAX - 1 && 
		resp->content_path[len - 1] != '/') {
		snprintf(index_path, PATH_MAX - 1, "%s/index.html", 
				resp->content_path);
	} else {
		snprintf(index_path, PATH_MAX - 1, "%sindex.html",
				resp->content_path);
	}

	if (access(index_path, F_OK) == 0) {
		if (stat(index_path, &index_st) < 0) {
			resp->status = STAT_INTERNAL_SERVER_ERROR;
			err_header_generator(resp);
			return;		
		}

		/* index.html exists in the directory */
		if (S_ISREG(index_st.st_mode)) {
			/* index.html read permission denied */
			if (access(index_path, R_OK) != 0) {
				resp->status = STAT_FORBIDDEN;
				err_header_generator(resp);
				return;
			}

			strcpy(resp->content_path, index_path);
			resp->lm_date = index_st.st_mtime;
			gmt_transfer(&resp->lm_date);
			resp->content_len = (uintmax_t)index_st.st_size;

			/* If-Modified-Since */
			if (req->method == GET && req->is_ims == SPECIFIED) {
				ims = mktime(&req->if_modified_since);
				if (ims < resp->curr_date && resp->lm_date < ims) {
					resp->status = STAT_NOT_MODIFIED;
					err_header_generator(resp);
					return;
				}
			}

			len = strlen(req->raw_uri);
			if (len > 0 && req->raw_uri[len - 1] != '/') {
				resp->status = STAT_MOVED_PERMANENTLY;
				err_header_generator(resp);
				return;
			}

			resp->content_type = CONTENT_TYPE_HTML;
			return;
		}
		
	}

	/* index.html not exist in the directory, show the list of 
	 * files in the directory.
	 */

	resp->ls_type = DIRTYPE;
	resp->lm_date = st.st_mtime;
	gmt_transfer(&resp->lm_date);
	resp->content_len = (uintmax_t)dir_content_len(resp->content_path,
												resp->raw_uri);
	if (resp->content_len == -1) {
		resp->status = STAT_INTERNAL_SERVER_ERROR;
		err_header_generator(resp);
	}

	resp->content_type = CONTENT_TYPE_HTML;

}

static void
err_header_generator(RESPONSE *resp) {
	resp->content_len = (uintmax_t)err_length(resp->status);
	resp->content_type = CONTENT_TYPE_HTML;
}

/* return -1 when internal server errors occur,
 * otherwise return the length of the content
 */
static off_t
dir_content_len(char *path, char *raw_uri) {
	off_t len;
	off_t frame, file, directory;
	FTS *handle;
	FTSENT *list, *child;
	char *argv[] = {path, NULL};

	frame = (off_t)strlen("<!DOCTYPE html>\r\n"
			"<html>\r\n"
			"\t<head>\r\n"
			"\t\t<title></title>\r\n"
			"\t</head>\r\n"
			"\t<body>\r\n"
			"\t\t<h1></h1>\r\n"
			"\t\t<ul>\r\n"
			"\t\t</ul>\r\n"
			"\t</body>\r\n"
			"</html>\r\n");

	file = (off_t)strlen("\t\t\t<li></li>\r\n");
	directory = (off_t)strlen("\t\t\t<li><a href=\"\"></a></li>\r\n");

	len = frame + 2 * strlen(raw_uri);

	if ((handle = fts_open(argv, FTS_SEEDOT, fts_cmp)) == NULL) {
		return -1;
	}

	while ((child = fts_read(handle)) != NULL) {
		if (child->fts_info == FTS_ERR || child->fts_info == FTS_NS
			|| child->fts_info == FTS_DC || child->fts_info == FTS_DNR) {
			fts_close(handle);
			return -1;
		}

		if (child->fts_info == FTS_D || child->fts_info == FTS_DOT) {
			if ((list = fts_children(handle,0)) == NULL) {
				if (errno == 0) { /* empty directory */
					break;
				} else {
					fts_close(handle);
					return -1;
				}
			}

			for (; list != NULL; list = list->fts_link) {
				if (list->fts_info == FTS_ERR || list->fts_info == FTS_NS || 
					list->fts_info == FTS_DC || list->fts_info == FTS_DNR) {
					fts_close(handle);
					return -1;
				}
				if (list->fts_info == FTS_DP) {
					continue;
				}

				if (list->fts_info == FTS_D || list->fts_info == FTS_DOT) {
					len += 2 * list->fts_namelen + directory;
				} else {
					len += list->fts_namelen + file;
				}
			}

			if (fts_set(handle, child, FTS_SKIP) == -1) {
				fts_close(handle);
				return -1;
			}
		}
	}

	fts_close(handle);
	return len;
}

static int
fts_cmp(const FTSENT **p, const FTSENT **q)
{
	unsigned short info_p, info_q;

	info_p = (*p)->fts_info;
	info_q = (*q)->fts_info;

	/* error file comes first */
	if (info_p == FTS_ERR)
		return  -1;
	else if (info_q == FTS_ERR)
		return 1;
	else if (info_p == FTS_NS || info_q == FTS_NS) {
		if (info_q != FTS_NS)
			return -1;
		else if (info_p != FTS_NS)
			return 1;

		return cmp_lexico(*p, *q);

	}

	if (info_p == FTS_DOT && info_q == FTS_DOT) {
		return cmp_lexico(*p, *q);
	} else if (info_p == FTS_DOT) {
		return -1;
	} else if (info_q == FTS_DOT) {
		return 1;
	}

	if (info_p == FTS_D && info_q != FTS_D) {
		return -1;
	} else if (info_p != FTS_D && info_q == FTS_D) {
		return 1;
	}

	return cmp_lexico(*p, *q);

}

static int
cmp_lexico(const FTSENT *p, const FTSENT *q)
{
	return strcmp(p->fts_name, q->fts_name);
}

static void
gmt_transfer(time_t *t) {
	time_t p;

	p = *t;
	*t = mktime(gmtime(&p));
}

static int
send_header(int msgsock, RESPONSE *resp, int cgi) {
	int len;
	char str[100];

	bzero(str, sizeof(str));

	/* status line */
	if (write_str(msgsock, "HTTP/1.0 ") == -1)
		return -1;

	switch (resp->status) {
		case STAT_OK:
			if (write_str(msgsock, "200 OK\r\n") == -1)
				return -1;
			break;
		case STAT_CREATED:
			if (write_str(msgsock, "201 Created\r\n") == -1)
				return -1;
			break;
		case STAT_ACCEPTED:
			if (write_str(msgsock, "202 Accepted\r\n") == -1)
				return -1;
			break;
		case STAT_NO_CONTENT:
			if (write_str(msgsock, "204 No Content\r\n") == -1)
				return -1;
			break;
		case STAT_MOVED_PERMANENTLY:
			if (write_str(msgsock, "301 Moved Permanently\r\n") == -1)
				return -1;
			break;
		case STAT_MOVED_TEMPORARILY:
			if (write_str(msgsock, "302 Moved Temporarily\r\n") == -1)
				return -1;
			break;
		case STAT_NOT_MODIFIED:
			if (write_str(msgsock, "304 Not Modified\r\n") == -1)
				return -1;
			break;
		case STAT_BAD_REQUEST:
			if (write_str(msgsock, "400 Bad Request\r\n") == -1)
				return -1;
			break;
		case STAT_UNAUTHORIZED:
			if (write_str(msgsock, "401 Unauthorized\r\n") == -1)
				return -1;
			break;
		case STAT_FORBIDDEN:
			if (write_str(msgsock, "403 Forbidden\r\n") == -1)
				return -1;
			break;
		case STAT_NOT_FOUND:
			if (write_str(msgsock, "404 Not Found\r\n") == -1)
				return -1;
			break;
		case STAT_REQUEST_TIMEOUT:
			if (write_str(msgsock, "408 Request Timeout\r\n") == -1)
				return -1;
			break;
		case STAT_INTERNAL_SERVER_ERROR:
			if (write_str(msgsock, "500 Internal Server Error\r\n") 
				== -1)
				return -1;
			break;
		case STAT_NOT_IMPLEMENTED:
			if (write_str(msgsock, "501 Not Implemented\r\n") == -1)
				return -1;
			break;
		case STAT_BAD_GATEWAY:
			if (write_str(msgsock, "502 Bad Gateway\r\n") == -1)
				return -1;
			break;
		case STAT_SERVICE_UNAVAILABLE:
			if (write_str(msgsock, "503 Service Unavailable\r\n") 
				== -1)
				return -1;
			break;
		default:
			return -1;
	}

	/* Date header */
	if (write_str(msgsock, "Date: ") == -1)
		return -1;

	len = strlen(ctime(&resp->curr_date));
	if (write(msgsock, ctime(&resp->curr_date), len - 1) != len - 1)
		return -1;

	if (write_str(msgsock, CRLF) == -1)
		return -1;

	/* Server header */
	if (write_str(msgsock, "Server: ") == -1)
		return -1;

	if (write_str(msgsock, SERVER_NAME) == -1)
		return -1;

	if (write_str(msgsock, CRLF) == -1)
		return -1;

	/* Last Modified header */
	if (resp->status == STAT_OK && cgi == NOT_SPECIFIED) {
		if (write_str(msgsock, "Last-Modified: ") == -1)
			return -1;

		len = strlen(ctime(&resp->lm_date));
		if (write(msgsock, ctime(&resp->lm_date), len - 1) != len - 1)
			return -1;

		if (write_str(msgsock, CRLF) == -1)
			return -1;		
	}

	if (resp->status == STAT_MOVED_PERMANENTLY ||
		resp->status == STAT_MOVED_TEMPORARILY) {
		if (write_str(msgsock, "Location: ") == -1)
			return -1;

		if (write_str(msgsock, resp->raw_uri) == -1)
			return -1;

		if (write_str(msgsock, CRLF) == -1)
			return -1;		
	}

	/* Content Type and Length header */
	if (resp->status != STAT_OK || cgi == NOT_SPECIFIED) {
		if (write_str(msgsock, "Content-Type: ") == -1)
			return -1;

		if (write_str(msgsock, resp->content_type) == -1)
			return -1;

		if (write_str(msgsock, CRLF) == -1)
			return -1;

		if (write_str(msgsock, "Content-Length: ") == -1)
			return -1;

		snprintf(str, 99, "%"PRIuMAX, resp->content_len);
		str[99] = '\0';

		if (write_str(msgsock, str) == -1)
			return -1;

		if (write_str(msgsock, CRLF) == -1)
			return -1;		

		/* CRLF between headers and entity body */
		if (write_str(msgsock, CRLF) == -1)
			return -1;
	}

	return 0;
}

static void
send_content(int msgsock, RESPONSE *resp, CGI_ENV *cgi, int is_cgi) {
	char buff[CONTENT_BUFSIZE];
	char *str;
	int fd;
	int n;

	bzero(buff, sizeof(buff));
	/* error */
	if (resp->status != STAT_OK) {
		send_err_content(msgsock, resp->status);
		if (resp->status == STAT_MOVED_TEMPORARILY ||
			resp->status == STAT_MOVED_PERMANENTLY) {
			if (write_str(msgsock, resp->raw_uri) == -1)
				return;

			str = "\">here</a>\r\n"
				"\t</body>\r\n"
				"</html>\r\n";
			write_str(msgsock, str);
			return;
		}
		return;
	}

	/* cgi */
	if (is_cgi == SPECIFIED) {
		cgi_exec(msgsock, cgi);
		return;
	}

	/* file */
	if (resp->ls_type == FILETYPE) {
		if ((fd = open(resp->content_path, O_RDONLY)) < 0)
			return;

		while ((n = read(fd, buff, CONTENT_BUFSIZE)) > 0) {
			if (write(msgsock, buff, n) != n)
				return;
		}

		return;
	}

	/* directory */
	send_dir_content(msgsock, resp->content_path, resp->raw_uri);
}

static void
send_dir_content(int msgsock, char *path, char *raw_uri) {
	FTS *handle;
	FTSENT *list, *child;
	char *argv[] = {path, NULL};

	if (write_str(msgsock, "<!DOCTYPE html>\r\n"
							"<html>\r\n"
							"\t<head>\r\n"
							"\t\t<title>") == -1)
		return;

	if (write_str(msgsock, raw_uri) == -1)
		return;

	if (write_str(msgsock, "</title>\r\n"
							"\t</head>\r\n"
							"\t<body>\r\n"
							"\t\t<h1>") == -1)
		return;

	if (write_str(msgsock, raw_uri) == -1)
		return;

	if (write_str(msgsock, "</h1>\r\n"
							"\t\t<ul>\r\n") == -1)
		return;


	if ((handle = fts_open(argv, FTS_SEEDOT, fts_cmp)) == NULL) {
		return;
	}

	while ((child = fts_read(handle)) != NULL) {
		if (child->fts_info == FTS_ERR || child->fts_info == FTS_NS
			|| child->fts_info == FTS_DC || child->fts_info == FTS_DNR) {
			fts_close(handle);
			return;
		}

		if (child->fts_info == FTS_D || child->fts_info == FTS_DOT) {
			if ((list = fts_children(handle,0)) == NULL) {
				if (errno == 0) { /* empty directory */
					break;
				} else {
					fts_close(handle);
					return;
				}
			}

			for (; list != NULL; list = list->fts_link) {
				if (list->fts_info == FTS_ERR || list->fts_info == FTS_NS || 
					list->fts_info == FTS_DC || list->fts_info == FTS_DNR) {
					fts_close(handle);
					return;
				}
				if (list->fts_info == FTS_DP) {
					continue;
				}

				if (strlen(list->fts_name) > 0 && list->fts_name[0] == '.' &&
					list->fts_info != FTS_DOT)
					continue;

				if (list->fts_info == FTS_D || list->fts_info == FTS_DOT) {
					if (write_str(msgsock, "\t\t\t<li><a href=\"") == -1)
						return;

					if (list->fts_info != FTS_DOT) {

						if (write_str(msgsock, "./") == -1)
							return;
					}

					if (write_str(msgsock, list->fts_name) == -1)
						return;

					if (write_str(msgsock, "/\">") == -1)
						return;

					if (write_str(msgsock, list->fts_name) == -1)
						return;

					if (write_str(msgsock, "</a></li>\r\n") == -1)
						return;
				} else {
					if (write_str(msgsock, "\t\t\t<li>") == -1)
						return;

					if (write_str(msgsock, list->fts_name) == -1)
						return;

					if (write_str(msgsock, "</li>\r\n") == -1)
						return;
				}
			}

			if (fts_set(handle, child, FTS_SKIP) == -1) {
				fts_close(handle);
				return;
			}
		}
	}

	if (write_str(msgsock, "\t\t</ul>\r\n"
							"\t</body>\r\n"
							"</html>\r\n") == -1)
		return;


	fts_close(handle);
}