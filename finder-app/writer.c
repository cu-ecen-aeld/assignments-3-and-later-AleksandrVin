#include <stdio.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>

int main(int argc, char ** argv)
{
	openlog(NULL, 0, LOG_USER);

	if(argc != 3)
	{
		syslog(LOG_ERR, "wrong number of argv, shoud be 3, provided %d", argc);
		return 1;
	}

	int fd = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC);
	if(fd == -1)
	{
		syslog(LOG_ERR, "can't open file");
		return 1;
	}
	if(write(fd, argv[2], strlen(argv[2])) == -1)
	{
		syslog(LOG_ERR, "can't write to file");
		return 1;
	}

	syslog(LOG_DEBUG, "Writing %s to %s", argv[1], argv[2]);

	close(fd);
	closelog();
	return 0;
}
