#include "sws.h"
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
char* setSlash(char* path,char* result);
int belong_to_cgi(char* path){
// 	int i = 0;
	// didn't consider /cgi/path1/ this kind of path yet.
	// while(1){
// 		if(path[i] == '\n') return 1;
// 		else if(path[i] != cgi_dir[i]) return 0;
// 		i++;
// 	}
	return 0;
}
void change_user_path(char* path){
	int i;
	int found = 0;
	char path_change[MAX_FILE_NAME];
	for(i = 0;path[i] != '\0';i++){
		if(path[i] == '~'){
			i++;
			found = 1;
			strcat(path_change,"/home/");
			int j;
			for(j = 6;path[i]!= '/';j++){
				path_change[j] = path[i];
				i++;
			}
			strcat(path_change,"/sws/");
			j = j+5;
			i++;
			for(;path[i]!='\0';j++){
				path_change[j] = path[i];
				i++;
			}
			break;
		}
	}
	if(found)
	strcpy(path,path_change);
}
void errorPrint(char *s){
	fprintf(stderr,"error:%s",s);
	 exit(EXIT_FAILURE);
}
int open_file(char* path,RESPONSE *response,int *fd){
	struct stat buf;
	change_user_path(path);
	if(stat(path,&buf)<0) {
		if(errno == 2) return 404;
		return 500;
	}
	if(!S_ISDIR(buf.st_mode)){
		int fileD;
		if((fileD = open(path,O_RDWR)) == -1){
			return 404;
		}
		response->lm_date = *gmtime(&buf.st_mtime);
		response->content_len = buf.st_size;
		strcpy(response->content_path,path);
		*fd = fileD;
		return 200;
	}else{
		struct dirent *entry;
		DIR *dirptr = NULL;
		if((dirptr = opendir(path)) == NULL)
		{
			return 404;
		}
		char path2[MAX_FILE_NAME];
		setSlash(path,path2);
		while((entry = readdir(dirptr)) != NULL){
			if(strcmp(entry->d_name,"index.html") == 0) {
				strcat(path2,"index.html");
				return open_file(path2,response,fd);
			}
		}
		return 600;
	}


}
int read_file(int fileD,char *buf,int size){
	int read_size;
	read_size = read(fileD,buf,size);
	return read_size;
}
char* setSlash(char* path,char* result){
	int i;

	if(strlen(path) == 0){
		printf("this path is null\n");
		strcpy(result,"/");
		return result;
	}else{
		char prev = '*';
		for(i = 0;path[i] != '\0';i++){
			prev = path[i];
		}
		if(prev == '/'){
			strcpy(result,path);
			return result;
		}else{
			strcpy(result,path);
			strcat(result,"/");
			return result;
		}
	}

}
FILE_TYPE *InsertSort(FILE_TYPE *head)  
{  
		FILE_TYPE *first; 
		FILE_TYPE *t; 
		FILE_TYPE *p; 
		FILE_TYPE *q; 
  
    first = head->next;
    head->next = NULL; 
  
    while (first != NULL) 
    {  
       
        for (t = first, q = head; q != NULL && strcmp(t->name,q->name)>0; p = q, q = q->next);
      


        first = first->next; 
    
        if (q == head) 
        {  
            head = t;  
        }  
        else 
        {  
            p->next = t;  
        }  
        t->next = q; 

    }  
    return head;  
}  
int disp_dir(char *path, FILE_TYPE **files){
	struct dirent *entry;
	DIR *dirptr = NULL;
	*files = malloc(sizeof(FILE_TYPE));
	change_user_path(path);
	if((dirptr = opendir(path)) == NULL)
	{
		return 404;
	}
	char path2[MAX_FILE_NAME];
	setSlash(path,path2);
	FILE_TYPE *scout = *files;


	FILE_TYPE *prev = NULL;


	while((entry = readdir(dirptr)) != NULL){

		if((entry->d_name)[0] == '.') continue;

		struct stat info;
		strcpy(scout->name,entry->d_name);
		char dst[MAX_FILE_NAME];
		strcpy(dst,path2);
		strcat(dst,entry->d_name);
		if(stat(dst,&info)<0){
			return 500;
		}
		scout->modified_datetime = *gmtime(&info.st_mtime);
		scout->size = info.st_size;
		if(S_ISDIR(info.st_mode))
			scout->type = 0;
		else
			scout->type = 1;
		prev = scout;
		FILE_TYPE *newOne = malloc(sizeof(FILE_TYPE));
		scout->next = newOne;
		scout = scout->next;
	}
	if(prev != NULL) prev->next = NULL;
	//*files = (*files)->next;
	*files = InsertSort(*files);
	return 0;
}
int get_lm_datetime(char* path,struct tm *time){
	struct stat buf;
	change_user_path(path);

	time = malloc(sizeof(struct tm));
	if(stat(path,&buf)<0) {
		if(errno == 2) return 404;
		return 500;
	}
	//printf("time:%ld\n",(buf.st_mtimespec).tv_sec);


	*time = *gmtime(&buf.st_mtime);
	return 0;
}

#if 0
int main(int argc,char **argv){
	/* test open and read */

	int fileD = -1;

	char buf[10];
	RESPONSE response;
	int return_number;
	return_number = open_file(argv[1],&response,&fileD);
	printf("the return number is :%d\n",return_number);
	while(read_file(fileD,buf,10)>0)
		write(STDOUT_FILENO,buf,10);
	printf("\n");
	//printf("lm time:%ld\n",response.lm_date.tv_sec);
	printf("content len:%ju\n",response.content_len);
	printf("content_path:%s\n",response.content_path);

	 test disp_dir*/

	/*
	FILE_TYPE *scout = malloc(sizeof(FILE_TYPE));
	disp_dir(argv[1],scout);
	while(scout != NULL){
	printf("file1 name: %s\n",scout->name);
	printf("type is   : %d\n",scout->type);
	scout = scout->next;
	}
	*/
	/** test get_lm_datetime*/
	/*
	struct timespec time;
	get_lm_datetime(argv[1],&time);
	printf("time:%ld\n",time.tv_sec);
	*/
}
#endif

