#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* i2c_test r addr
* i2c_test w addr val
*/

void print_usage(char *file)
{
	printf("%s eeprom: r addr1 addr2 len\n", file);
	printf("%s eeprom: w addr1 addr2 val1 val2 ...\n", file);
}

void print_buf(char *buf_r, int len, int addr1, int addr2)
{
	int i;
	for (i = 0; i < len; i++) {
		printf("addr:0x%02x%02x val:%02x\n", addr1, addr2+i, *buf_r);
		buf_r++;
	}
}

void get_buf(char **argv, char *buf_w, int len)
{
	int i;
	for(i = 0; i < len; i++) {
		*buf_w = strtoul(argv[i+2], NULL, 0);
		buf_w++;
	}
}

int main(int argc, char **argv)
{
	int fd;
	int i, len, addr1, addr2;
	char *path = "/dev/eeprom-ldq";
	unsigned char *buf_w, *buf_r;

	fd = open(path, O_RDWR);
	printf("fd :%d\n",fd);

	if (argc <= 4)
	{
		print_usage(argv[0]);
		return -1;
	}

	if (fd < 0)
	{
		printf("can't open %s\n", path);
		return -1;
	}

	if (strcmp(argv[1], "r") == 0)
	{
		printf("read\n");

		len = strtoul(argv[4], NULL, 0);
		addr1 = strtoul(argv[2], NULL, 0);
		addr2 = strtoul(argv[3], NULL, 0);

		buf_w = malloc(2);
		*buf_w = addr1;
		buf_w++;
		*buf_w = addr2;
		buf_w--;
		write(fd, buf_w, 2);
		free(buf_w);

		buf_r = malloc(len);
		read(fd, buf_r, len);
		print_buf(buf_r, len, addr1, addr2);
		free(buf_r);
	} else if (strcmp(argv[1], "w") == 0)
	{
		printf("write\n");

		len = argc - 2;

		buf_w = malloc(len);
		get_buf(argv, buf_w, len);
		write(fd, buf_w, len);
		free(buf_w);
	} else {
		print_usage(argv[0]);
		return -1;
	}
	return 0;
}
