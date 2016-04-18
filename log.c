#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
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
/*	struct tm *info;
	char buf_time[20]; // max time size in format is 19
	char *buff;
	char stat[4];
	char size[20];
	int len;
	struct flock lock;

	bzero(buf_time, 20);
	memset(&lock, 0, sizeof lock);

	if (log_info->l_time == (time_t)-1) {
		strcpy(buf_time, "??:??:?? ?\?-?\?-????");
	} else {
		info = gmtime(&log_info->l_time);
		strftime(buf_time, sizeof buf_time, "%H:%M:%S %m-%d-%Y",info);
	}
	
	if(!dflag && !lflag)
		return;

	// doing time 
	if(dflag) {
		printf("%s %s \"%s\" %d %d\n",log_info->l_addr, buf_time,
		  log_info->l_line, log_info->l_req_stat, log_info->l_size);
		return;
	}
	
	if(lflag) {

		sprintf(stat, "%d", log_info->l_req_stat);
		sprintf(size, "%d ", log_info->l_size);

		len = strlen(log_info->l_addr) + 1 +
		      strlen(buf_time) + 1 +
		      strlen(log_info->l_line) + 3 +
		      strlen(stat) + 1 +
		      strlen(size) + 2;
	
		buff = (char *)calloc(len + 1, sizeof (char));

		sprintf(buff, "%s %s \"%s\" %s %s\r\n",
			log_info->l_addr, buf_time, log_info->l_line,
			stat, size);
		buff[len] = '\0';
		lock.l_type = F_WRLCK;
		lock.l_start = 0;
		lock.l_len = 0;
		lock.l_whence = SEEK_END;
		
		if (fcntl(fd_log, F_SETLKW, &lock) == -1)
			return;


		if (write(fd_log, buff, len) < 0)
			exit(EXIT_FAILURE);

		lock.l_type = F_UNLCK;
		if (fcntl(fd_log, F_SETLKW, &lock) == -1)
			exit(EXIT_FAILURE);

	}	*/
}
