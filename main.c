#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <locale.h>

#include "sws.h"

#define DEFAULT_PORT "8080"

/* flags */
int cflag;	/* CGI */
int dflag;	/* debugging */
int hflag;	/* help */
int iflag;	/* ip address */
int lflag;	/* log */
int pflag;	/* port */
char doc_root[PATH_MAX];
char cgi_dir[PATH_MAX], log_file[PATH_MAX];
char *server_addr, *port;
char *progname;

static void usage();
static int check_file(const char *, int);

int
main(int argc, char **argv)
{
	int opt;
	int len;
	progname = argv[0];
	(void)setlocale(LC_ALL, "");
	port = DEFAULT_PORT;

	bzero(doc_root, sizeof(doc_root));
	bzero(cgi_dir, sizeof(cgi_dir));
	bzero(log_file, sizeof(log_file));

	while ((opt = getopt(argc, argv, "c:dhi:l:p:")) != -1) {
		switch (opt) {
		case 'c':
			cflag = 1;
			/* last character reserved for trailing slash */
			strncpy(cgi_dir, optarg, PATH_MAX - 2);
			cgi_dir[PATH_MAX - 1] = '\0';
			len = strlen(cgi_dir);
			/* add a trailing slash */
			if (len > 0 && cgi_dir[len - 1] != '/')
				cgi_dir[len] = '/';
			break;
		case 'd':
			dflag = 1;
			lflag = 0;
			break;
		case 'i':
			iflag = 1;
			server_addr = optarg;
			break;
		case 'l':
			lflag = 1;
			dflag = 0;
			strncpy(log_file, optarg, PATH_MAX - 1);
			log_file[PATH_MAX - 1] = '\0';
			break;
		case 'p':
			pflag = 1;
			port = optarg;
			break;
		case 'h':		/* print usage and exit */
			hflag = 1;
		default:
			usage();
		}
	} 

	argc -= optind;
	argv += optind;

	if (argc == 0) {
		fprintf(stderr, "Document root: not specified\n");
		return EXIT_FAILURE;
	}

	/* last character reserved for trailing slash */
	strncpy(doc_root, argv[0], PATH_MAX - 2);
	doc_root[PATH_MAX - 1] = '\0';
	len = strlen(doc_root);
	/* add a trailing slash */
	if (len > 0 && doc_root[len - 1] != '/')
		doc_root[len] = '/';

	if (atoi(port) < 1024 || atoi(port) > 65535) {
		fprintf(stderr, "Port number: %s: Not Valid (1024~65535)\n", port);
		return EXIT_FAILURE;
	}

	if (cflag) {
		if (check_file(cgi_dir, DIRTYPE) == -1) {
			return EXIT_FAILURE;
		}
	}

	if (lflag) {
		if (check_file(log_file, FILETYPE) == -1) {
			return EXIT_FAILURE;
		}
	}

	if (check_file(doc_root, DIRTYPE) == -1) {
		return EXIT_FAILURE;
	}

	/* debugging 
	dflag = 1;
	lflag = 0;
*/
	start_server();

	return EXIT_SUCCESS;
}

static void
usage()
{
	fprintf(stderr, "Usage: ./%s [-dh][-c dir][-i address][-l file]"
			"[-p port] dir\n", progname);
	if (hflag) {
		fprintf(stderr, "-c dir\t\tAllow execution of CGIs from the"
				"given directory.\n");
		fprintf(stderr, "-d\t\tEnter debugging mode.\n");
		fprintf(stderr, "-h\t\tPrint a short usage summary and exit."
				"\n");
		fprintf(stderr, "-i address\tBind to the given IPv4 or IPv6"
				"address.\n");
		fprintf(stderr, "-l file\t\tLog all requests to the given file."
				"\n");
		fprintf(stderr, "-p port\t\tListen on the given port.\n");

		exit(EXIT_SUCCESS);
	}

	exit(EXIT_FAILURE);
}

/*
 * Return -1 on error. Return 0 on success.
 */
static int
check_file(const char *pathname, int fileType)
{
    int fd;
    struct stat st;
    if (fileType == FILETYPE) {	/* log file */
        if ((fd = open(pathname, O_RDWR | O_CREAT | O_APPEND, S_IWUSR | S_IRUSR))
            < 0) {
            fprintf(stderr, "%s: Unable to open Log file: %s\n",
                    pathname, strerror(errno));
            return -1;
        }
        
        (void)close(fd);

        return 0;
    } else {	/* CGI directory or document root */
        if (stat(pathname, &st) < 0) {
            fprintf(stderr, "%s: Unable to stat: %s\n", pathname,
                    strerror(errno));
            return -1;
        }
        switch (st.st_mode & S_IFMT) {
            case S_IFDIR:
                if (access(pathname, R_OK) == -1) {
                    fprintf(stderr, "%s: Unable to read:"
                                    " %s\n", pathname,
                            strerror(errno));
                    return -1;
                }
                return 0;
            default:
                fprintf(stderr, "%s: Not a valid DIR\n",
                        pathname);
                return -1;
        }
    }
}
