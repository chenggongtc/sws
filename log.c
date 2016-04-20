#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "sws.h"

void
ini_log(LOG  *log_info) {
	log_info->status = STAT_NOT_SPECIFIED;
	log_info->size = 0;
	bzero(log_info->l_addr, sizeof log_info->l_addr);
	log_info->l_time = (time_t)0;
	bzero(log_info->l_line, sizeof log_info->l_line);
}

void 
log_sws(LOG *log_info) {
	int fd;
	char str[50];
	int len;

	bzero(str, sizeof(str));

	if (dflag) {
		if (strlen(log_info->l_addr) == 0)
			printf("UNKNOW ");
		else
			printf("%s ", log_info->l_addr);

		if (log_info->l_time == (time_t)(-1))
			printf("UNKNOW ");
		else
			printf("%.24s ", ctime(&log_info->l_time));

		if (strlen(log_info->l_line) == 0)
			printf("\"UNKNOW\" ");
		else
			printf("\"%s\" ", log_info->l_line);

		printf("%d ", log_info->status);

		printf("%"PRIuMAX"\n", log_info->size);

		fflush(stdout);
	}

	if (lflag) {
		if ((fd = open(log_file, O_WRONLY | O_APPEND)) == -1)
			return;

		if (flock(fd, LOCK_EX) < 0) {
			close(fd);
			return;
		}

		if (strlen(log_info->l_addr) == 0) {
			if (write_str(fd, "UNKNOW ") == -1) {
				flock(fd, LOCK_UN);
				close(fd);
				return;
			}
		} else{
			if (write_str(fd, log_info->l_addr) == -1){
				flock(fd, LOCK_UN);
				close(fd);
				return;
			}
			if (write_str(fd, " ") == -1) {
				flock(fd, LOCK_UN);
				close(fd);
				return;
			}
		}
		if (log_info->l_time == (time_t)(-1)) {
			if (write_str(fd, "UNKNOW ") == -1) {
				flock(fd, LOCK_UN);
				close(fd);
				return;
			}
		} else {
			strcpy(str, ctime(&log_info->l_time));
			len = strlen(str) - 1;
			if (write(fd, str, len) != len) {
				flock(fd, LOCK_UN);
				close(fd);
				return;
			}
			if (write_str(fd, " ") == -1) {
				flock(fd, LOCK_UN);
				close(fd);
				return;
			}
		}
			

		if (strlen(log_info->l_line) == 0) {
			if (write_str(fd, "UNKNOW ") == -1) {
				flock(fd, LOCK_UN);
				close(fd);
				return;
			}
		} else {
			if (write_str(fd, "\"") == -1) {
				flock(fd, LOCK_UN);
				close(fd);
				return;
			}
			if (write_str(fd, log_info->l_line) == -1) {
				flock(fd, LOCK_UN);
				close(fd);
				return;
			}
			if (write_str(fd, "\" ") == -1) {
				flock(fd, LOCK_UN);
				close(fd);
				return;
			}
		}

		snprintf(str, 50, "%d %"PRIuMAX"\n", log_info->status,
											log_info->size);
		if (write_str(fd, str) == -1) {
			flock(fd, LOCK_UN);
			close(fd);
			return;
		}

		flock(fd, LOCK_UN);
		close(fd);
	}
}