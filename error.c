#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "sws.h"

int
err_length(int status_code)
{
	switch (status_code) {
	case STAT_MOVED_PERMANENTLY:
		return strlen("<!DOCTYPE html>\r\n"
			"<html>\r\n"
			"\t<head>\r\n"
			"\t\t<title>301 Moved Permanently</title>\r\n"
			"\t</head>\r\n"
			"\t<body>\r\n"
			"\t\t<h1>Moved Permanently</h1>\r\n"
			"\t</body>\r\n"
			"</html>\r\n");
	case STAT_MOVED_TEMPORARILY:
		return strlen("<!DOCTYPE html>\r\n"
			"<html>\r\n"
			"\t<head>\r\n"
			"\t\t<title>302 Moved Temporarily</title>\r\n"
			"\t</head>\r\n"
			"\t<body>\r\n"
			"\t\t<h1>Moved Temporarily</h1>\r\n"
			"\t</body>\r\n"
			"</html>\r\n");
	case STAT_NOT_MODIFIED:
		return strlen("<!DOCTYPE html>\r\n"
			"<html>\r\n"
			"\t<head>\r\n"
			"\t\t<title>304 Not Modified</title>\r\n"
			"\t</head>\r\n"
			"\t<body>\r\n"
			"\t\t<h1>Not Modified</h1>\r\n"
			"\t</body>\r\n"
			"</html>\r\n");
	case STAT_BAD_REQUEST:
		return strlen("<!DOCTYPE html>\r\n"
			"<html>\r\n"
			"\t<head>\r\n"
			"\t\t<title>400 Bad Request</title>\r\n"
			"\t</head>\r\n"
			"\t<body>\r\n"
			"\t\t<h1>Bad Request</h1>\r\n"
			"\t</body>\r\n"
			"</html>\r\n");
	case STAT_UNAUTHORIZED:
		return strlen("<!DOCTYPE html>\r\n"
			"<html>\r\n"
			"\t<head>\r\n"
			"\t\t<title>401 Unauthorized</title>\r\n"
			"\t</head>\r\n"
			"\t<body>\r\n"
			"\t\t<h1>Unauthorized</h1>\r\n"
			"\t</body>\r\n"
			"</html>\r\n");
	case STAT_FORBIDDEN:
		return strlen("<!DOCTYPE html>\r\n"
			"<html>\r\n"
			"\t<head>\r\n"
			"\t\t<title>403 Forbidden</title>\r\n"
			"\t</head>\r\n"
			"\t<body>\r\n"
			"\t\t<h1>Forbidden</h1>\r\n"
			"\t</body>\r\n"
			"</html>\r\n");
	case STAT_NOT_FOUND:
		return strlen("<!DOCTYPE html>\r\n"
			"<html>\r\n"
			"\t<head>\r\n"
			"\t\t<title>404 Not Found</title>\r\n"
			"\t</head>\r\n"
			"\t<body>\r\n"
			"\t\t<h1>Not Found</h1>\r\n"
			"\t</body>\r\n"
			"</html>\r\n");
	case STAT_REQUEST_TIMEOUT:
		return strlen("<!DOCTYPE html>\r\n"
			"<html>\r\n"
			"\t<head>\r\n"
			"\t\t<title>408 Request Timeout</title>\r\n"
			"\t</head>\r\n"
			"\t<body>\r\n"
			"\t\t<h1>Request Timeout</h1>\r\n"
			"\t</body>\r\n"
			"</html>\r\n");
	case STAT_INTERNAL_SERVER_ERROR:
		return strlen("<!DOCTYPE html>\r\n"
			"<html>\r\n"
			"\t<head>\r\n"
			"\t\t<title>500 Internal Server Error</title>\r\n"
			"\t</head>\r\n"
			"\t<body>\r\n"
			"\t\t<h1>Internal Server Error</h1>\r\n"
			"\t</body>\r\n"
			"</html>\r\n");
	case STAT_NOT_IMPLEMENTED:
		return strlen("<!DOCTYPE html>\r\n"
			"<html>\r\n"
			"\t<head>\r\n"
			"\t\t<title>501 Not Implemented</title>\r\n"
			"\t</head>\r\n"
			"\t<body>\r\n"
			"\t\t<h1>Not Implemented</h1>\r\n"
			"<\t/body>\r\n"
			"</html>\r\n");
	case STAT_BAD_GATEWAY:
		return strlen("<!DOCTYPE html>\r\n"
			"<html>\r\n"
			"\t<head>\r\n"
			"\t\t<title>502 Bad Gateway</title>\r\n"
			"\t</head>\r\n"
			"\t<body>\r\n"
			"\t\t<h1>Bad Gateway</h1>\r\n"
			"\t</body>\r\n"
			"</html>\r\n");
	case STAT_SERVICE_UNAVAILABLE:
		return strlen("<!DOCTYPE html>\r\n"
			"<html>\r\n"
			"\t<head>\r\n"
			"\t\t<title>503 Service Unavailable</title>\r\n"
			"\t</head>\r\n"
			"\t<body>\r\n"
			"\t\t<h1>Server Unavailable</h1>\r\n"
			"\t</body>\r\n"
			"</html>\r\n");
	default:
		return 0;
	}
}

void
send_err_content(int msgsock, int status_code)
{
	char *err_info;
	switch (status_code) {
	case STAT_MOVED_PERMANENTLY:
		err_info = "<!DOCTYPE html>\r\n"
			"<html>\r\n"
			"\t<head>\r\n"
			"\t\t<title>301 Moved Permanently</title>\r\n"
			"\t</head>\r\n"
			"\t<body>\r\n"
			"\t\t<h1>Moved Permanently</h1>\r\n"
			"\t</body>\r\n"
			"</html>\r\n";
		break;
	case STAT_MOVED_TEMPORARILY:
		err_info = "<!DOCTYPE html>\r\n"
			"<html>\r\n"
			"\t<head>\r\n"
			"\t\t<title>302 Moved Temporarily</title>\r\n"
			"\t</head>\r\n"
			"\t<body>\r\n"
			"\t\t<h1>Moved Temporarily</h1>\r\n"
			"\t</body>\r\n"
			"</html>\r\n";
		break;
	case STAT_NOT_MODIFIED:
		err_info = "<!DOCTYPE html>\r\n"
			"<html>\r\n"
			"\t<head>\r\n"
			"\t\t<title>304 Not Modified</title>\r\n"
			"\t</head>\r\n"
			"\t<body>\r\n"
			"\t\t<h1>Not Modified</h1>\r\n"
			"\t</body>\r\n"
			"</html>\r\n";
		break;
	case STAT_BAD_REQUEST:
		err_info = "<!DOCTYPE html>\r\n"
			"<html>\r\n"
			"\t<head>\r\n"
			"\t\t<title>400 Bad Request</title>\r\n"
			"\t</head>\r\n"
			"\t<body>\r\n"
			"\t\t<h1>Bad Request</h1>\r\n"
			"\t</body>\r\n"
			"</html>\r\n";
		break;
	case STAT_UNAUTHORIZED:
		err_info = "<!DOCTYPE html>\r\n"
			"<html>\r\n"
			"\t<head>\r\n"
			"\t\t<title>401 Unauthorized</title>\r\n"
			"\t</head>\r\n"
			"\t<body>\r\n"
			"\t\t<h1>Unauthorized</h1>\r\n"
			"\t</body>\r\n"
			"</html>\r\n";
		break;
	case STAT_FORBIDDEN:
		err_info = "<!DOCTYPE html>\r\n"
			"<html>\r\n"
			"\t<head>\r\n"
			"\t\t<title>403 Forbidden</title>\r\n"
			"\t</head>\r\n"
			"\t<body>\r\n"
			"\t\t<h1>Forbidden</h1>\r\n"
			"\t</body>\r\n"
			"</html>\r\n";
		break;
	case STAT_NOT_FOUND:
		err_info = "<!DOCTYPE html>\r\n"
			"<html>\r\n"
			"\t<head>\r\n"
			"\t\t<title>404 Not Found</title>\r\n"
			"\t</head>\r\n"
			"\t<body>\r\n"
			"\t\t<h1>Not Found</h1>\r\n"
			"\t</body>\r\n"
			"</html>\r\n";
		break;
	case STAT_REQUEST_TIMEOUT:
		err_info = "<!DOCTYPE html>\r\n"
			"<html>\r\n"
			"\t<head>\r\n"
			"\t\t<title>408 Request Timeout</title>\r\n"
			"\t</head>\r\n"
			"\t<body>\r\n"
			"\t\t<h1>Request Timeout</h1>\r\n"
			"\t</body>\r\n"
			"</html>\r\n";
		break;
	case STAT_INTERNAL_SERVER_ERROR:
		err_info = "<!DOCTYPE html>\r\n"
			"<html>\r\n"
			"\t<head>\r\n"
			"\t\t<title>500 Internal Server Error</title>\r\n"
			"\t</head>\r\n"
			"\t<body>\r\n"
			"\t\t<h1>Internal Server Error</h1>\r\n"
			"\t</body>\r\n"
			"</html>\r\n";
		break;
	case STAT_NOT_IMPLEMENTED:
		err_info = "<!DOCTYPE html>\r\n"
			"<html>\r\n"
			"\t<head>\r\n"
			"\t\t<title>501 Not Implemented</title>\r\n"
			"\t</head>\r\n"
			"\t<body>\r\n"
			"\t\t<h1>Not Implemented</h1>\r\n"
			"<\t/body>\r\n"
			"</html>\r\n";
		break;
	case STAT_BAD_GATEWAY:
		err_info = "<!DOCTYPE html>\r\n"
			"<html>\r\n"
			"\t<head>\r\n"
			"\t\t<title>502 Bad Gateway</title>\r\n"
			"\t</head>\r\n"
			"\t<body>\r\n"
			"\t\t<h1>Bad Gateway</h1>\r\n"
			"\t</body>\r\n"
			"</html>\r\n";
		break;
	case STAT_SERVICE_UNAVAILABLE:
		err_info = "<!DOCTYPE html>\r\n"
			"<html>\r\n"
			"\t<head>\r\n"
			"\t\t<title>503 Service Unavailable</title>\r\n"
			"\t</head>\r\n"
			"\t<body>\r\n"
			"\t\t<h1>Server Unavailable</h1>\r\n"
			"\t</body>\r\n"
			"</html>\r\n";
		break;
	default:
		err_info = NULL;
		return;
	}

	write(msgsock, err_info, strlen(err_info));
}