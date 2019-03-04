/*
redirect test
*/
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

int redir(const char *str)
{
	int out = open("cout.log", O_RDWR|O_CREAT|O_APPEND, 0600);

	if (-1 == out) {
		perror("opening cout.log");
		return 255;
	}

	int err = open("cerr.log", O_RDWR|O_CREAT|O_APPEND, 0600);
	if (-1 == err) {
		perror("opening cerr.log");
		return 255;
	}

	int save_out = dup(fileno(stdout));
	int save_err = dup(fileno(stderr));

	if (-1 == dup2(out, fileno(stdout))) {
		perror("cannot redirect stdout");
		return 255;
	}
	if (-1 == dup2(err, fileno(stderr))) {
		perror("cannot redirect stderr");
		return 255;
	}

	puts(str);

	fflush(stdout);
	close(out);

	fflush(stderr);
	close(err);

	dup2(save_out, fileno(stdout));
	dup2(save_err, fileno(stderr));

	close(save_out);
	close(save_err);

	//puts("back to normal output");
}

int main(int argc, const char *argv[])
{
	char *str = "shiguanghu test.";
	char *str2 = "shiguanghu test 2.";

	redir(str);
	redir(str2);

	return 0;
}
