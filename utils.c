#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* return 0 on success, otherwise -1 */
int
write_str(int fd, char *str) {
	int len;

	len = strlen(str);
	if (write(fd, str, len) != len)
		return -1;
	return 0;
}