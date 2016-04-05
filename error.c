/*
 * error.c		- handle error code formatting
 * sws			- a simple web server
 *
 * CS631 final project	- sws
 * Group 		- flag
 * Members 		- Chenyao Wang, cwang60@stevens.edu
 *			- Dongxu Han, dhan7@stevens.edu
 *			- Gong Cheng, gcheng2@stevens.edu
 *			- Maisi Li, mli27@stevens.edu
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sws.h"

char *
print_err(int status_code)
{
	switch (status_code) {
	case STAT_MOVED_PERMANENTLY:
		return "<!DOCTYPE html>\r\n"
			"<html>\r\n"
			"<head>\r\n"
			"<title>301 Moved Permanently</title>\r\n"
			"</head>\r\n"
			"<body>\r\n"
			"<h1>Moved Permanently</h1>\r\n"
			"</body>\r\n"
			"</html>\r\n";
	case STAT_MOVED_TEMPORARILY:
		return "<!DOCTYPE html>\r\n"
			"<html>\r\n"
			"<head>\r\n"
			"<title>302 Moved Temporarily</title>\r\n"
			"</head>\r\n"
			"<body>\r\n"
			"<h1>Moved Temporarily</h1>\r\n"
			"</body>\r\n"
			"</html>\r\n";
	case STAT_NOT_MODIFIED:
		return "<!DOCTYPE html>\r\n"
			"<html>\r\n"
			"<head>\r\n"
			"<title>304 Not Modified</title>\r\n"
			"</head>\r\n"
			"<body>\r\n"
			"<h1>Not Modified</h1>\r\n"
			"</body>\r\n"
			"</html>\r\n";
	case STAT_BAD_REQUEST:
		return "<!DOCTYPE html>\r\n"
			"<html>\r\n"
			"<head>\r\n"
			"<title>400 Bad Request</title>\r\n"
			"</head>\r\n"
			"<body>\r\n"
			"<h1>Bad Request</h1>\r\n"
			"</body>\r\n"
			"</html>\r\n";
	case STAT_UNAUTHORIZED:
		return "<!DOCTYPE html>\r\n"
			"<html>\r\n"
			"<head>\r\n"
			"<title>401 Unauthorized</title>\r\n"
			"</head>\r\n"
			"<body>\r\n"
			"<h1>Unauthorized</h1>\r\n"
			"</body>\r\n"
			"</html>\r\n";
	case STAT_FORBIDDEN:
		return "<!DOCTYPE html>\r\n"
			"<html>\r\n"
			"<head>\r\n"
			"<title>403 Forbidden</title>\r\n"
			"</head>\r\n"
			"<body>\r\n"
			"<h1>Forbidden</h1>\r\n"
			"</body>\r\n"
			"</html>\r\n";
	case STAT_NOT_FOUND:
		return "<!DOCTYPE html>\r\n"
			"<html>\r\n"
			"<head>\r\n"
			"<title>404 Not Found</title>\r\n"
			"</head>\r\n"
			"<body>\r\n"
			"<h1>Not Found</h1>\r\n"
			"</body>\r\n"
			"</html>\r\n";
	case STAT_REQUEST_TIMEOUT:
		return "<!DOCTYPE html>\r\n"
			"<html>\r\n"
			"<head>\r\n"
			"<title>408 Request Timeout</title>\r\n"
			"</head>\r\n"
			"<body>\r\n"
			"<h1>Request Timeout</h1>\r\n"
			"</body>\r\n"
			"</html>\r\n";
	case STAT_INTERNAL_SERVER_ERROR:
		return "<!DOCTYPE html>\r\n"
			"<html>\r\n"
			"<head>\r\n"
			"<title>500 Internal Server Error</title>\r\n"
			"</head>\r\n"
			"<body>\r\n"
			"<h1>Internal Server Error</h1>\r\n"
			"</body>\r\n"
			"</html>\r\n";
	case STAT_NOT_IMPLEMENTED:
		return "<!DOCTYPE html>\r\n"
			"<html>\r\n"
			"<head>\r\n"
			"<title>501 Not Implemented</title>\r\n"
			"</head>\r\n"
			"<body>\r\n"
			"<h1>Not Implemented</h1>\r\n"
			"</body>\r\n"
			"</html>\r\n";
	case STAT_BAD_GATEWAY:
		return "<!DOCTYPE html>\r\n"
			"<html>\r\n"
			"<head>\r\n"
			"<title>502 Bad Gateway</title>\r\n"
			"</head>\r\n"
			"<body>\r\n"
			"<h1>Bad Gateway</h1>\r\n"
			"</body>\r\n"
			"</html>\r\n";
	case STAT_SERVER_UNAVAILABLE:
		return "<!DOCTYPE html>\r\n"
			"<html>\r\n"
			"<head>\r\n"
			"<title>503 Server Unavailable</title>\r\n"
			"</head>\r\n"
			"<body>\r\n"
			"<h1>Server Unavailable</h1>\r\n"
			"</body>\r\n"
			"</html>\r\n";
	default:
		return NULL;
	}

}
