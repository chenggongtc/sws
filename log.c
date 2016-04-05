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
ini_conf(SERVERINFO* ser) {

	memset(ser, 0, sizeof &ser);
	bzero(ser->si_name, sizeof ser->si_name);
	bzero(ser->si_address, sizeof ser->si_address);
	bzero(ser->si_port, sizeof ser->si_port);
	bzero(ser->si_rootdir, sizeof ser->si_rootdir);
	bzero(ser->si_cgidir, sizeof ser->si_cgidir);
	bzero(ser->si_logpath, sizeof ser->si_logpath);
	bzero(ser->si_debugpath, sizeof ser->si_debugpath);
	bzero(ser->si_cflag, sizeof ser->si_cflag);
	bzero(ser->si_lflag, sizeof ser->si_lflag);
}



char *
trim(char *word) {
	char *begin = word, *end = &word[strlen(word) - 1];

	while(isspace(*end) && (end >= begin)) end--;
	*(end + 1) = '\0';

	while(isspace(*begin) && (begin < end)) begin++;
	strcpy(word, begin);
	
	return word;
	
}

void 
parse_conf(SERVERINFO *info) {
	int res = 0;
	char* conf_dir = "./var/server.conf";
	char *s, buf[256];
	FILE *fp = fopen(conf_dir,"r");
	
	if(fp == NULL) {
		fprintf(stderr, "fopen fail.\n");
		exit(1);
	}

	ini_conf(info);

	if((res = flock(fileno(fp), LOCK_EX)) < 0) {
		fprintf(stderr, "flock fail: %s\n",strerror(errno));
		exit(1);
	}
	bzero(buf, sizeof buf);
	while((s = fgets(buf, sizeof buf, fp)) != NULL) {
		if(buf[0] == '\n' || buf[0] == '#')
			continue;
		
		char name[NAME_MAX], value[NAME_MAX];
		
		s = trim(strtok(buf, "="));
		if(s == NULL)
			continue;
		//printf("%s\n", s);
		strncpy(name, s, NAME_MAX);
		
		s = trim(strtok(NULL, "="));
		strncpy(value, s, NAME_MAX);
		//printf("value: %s len: %d \n",value,strlen(value));
		if(strcmp(name, "ServerAddress") == 0)
 			strncpy(info->si_address, value, sizeof info->si_address);
		if(strcmp(name,"ServerPort") == 0)
			strncpy(info->si_port, value, sizeof info->si_port);
		if(strcmp(name,"ServerName") == 0)
			strncpy(info->si_name, value, NAME_MAX);
		if(strcmp(name,"CGIDir") == 0)
			strncpy(info->si_cgidir, value, sizeof info->si_cgidir);
		if(strcmp(name, "RootDir") == 0)
			strncpy(info->si_rootdir, value, sizeof info->si_rootdir);
		if(strcmp(name, "DebugFile") == 0)
			strncpy(info->si_debugpath, value, sizeof info->si_debugpath);
		if(strcmp(name, "LogFile") == 0)
			strncpy(info->si_logpath, value, sizeof info->si_logpath);
		if(strcmp(name, "Cflag") == 0)
			strncpy(info->si_cflag, value, sizeof info->si_cflag);
		if(strcmp(name, "Lflag") == 0) 
			strncpy(info->si_lflag, value, sizeof info->si_lflag);
	}

	if((res = flock(fileno(fp), LOCK_UN)) < 0) {
		fprintf(stderr, "unlock fail: %s\n", strerror(errno));
		exit(1);
	}
	fclose(fp);

}

void
ini_log(LOG  *log_info) {
	log_info->l_req_stat = 0;
	log_info->l_size = 0;
	bzero(log_info->l_addr, sizeof log_info->l_addr);
	log_info->l_time = (time_t)0;
	bzero(log_info->l_line, sizeof log_info->l_line);
}

void 
log_sws(LOG *log_info) {
	struct tm *info;
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

	}	
}
