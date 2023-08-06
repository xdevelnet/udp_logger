#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>

#define DEFAULT_FILE_MODE (S_IREAD | S_IWRITE | S_IRGRP | S_IROTH)

#define FMT_TIME_MAX_STR 40

#define NETWORK_UNAVAILABLE_WAIT 100

void new_sleep(unsigned seconds) {
	struct timespec t = {.tv_sec = seconds};
	nanosleep(&t, NULL);
}

long time_formatted(char str[FMT_TIME_MAX_STR]) {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	long msec = lrint(tv.tv_usec / 1000.0); // Round to nearest msec
	if (msec >= 1000) { // Allow for rounding up to nearest second
		msec -= 1000;
		tv.tv_sec++;
	}

	struct tm *tm_info = localtime(&tv.tv_sec);
	strftime(str, 26, "%d-%m-%Y %H:%M:%S", tm_info);
	// printf("%s.%03d\n", buffer, msec);
	return msec;
}

static size_t buf_to_print(const char *buffer, size_t len, char *printable_buffer) {
	size_t printable = 0;
	for (size_t i = 0; i < len; i++) {
		if (isprint(buffer[i])) {
			printable_buffer[printable] = buffer[i];
			printable++;
		}
	}

	return printable;
}

void worker(int fd) {
	char cur_time[FMT_TIME_MAX_STR];
	long msec;

	while(1) {
		int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

		if (sock < 0) {
			if (errno == EINTR) {
				dprintf(fd, "%s.%03li - Interrupting from socket() syscall\n", cur_time, msec);
				break;
			}

			msec = time_formatted(cur_time);
			dprintf(fd, "%s.%03li - Unable to create a socket: %s. Waiting for %d seconds\n", cur_time, msec, strerror(errno), NETWORK_UNAVAILABLE_WAIT);
			new_sleep(NETWORK_UNAVAILABLE_WAIT);
			continue;
		}

		int enable = 1;
		setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));

		struct sockaddr_in my_addr = {.sin_family = AF_INET, .sin_port = htons(8888), .sin_addr.s_addr = INADDR_ANY};

		if (bind(sock, (struct sockaddr *) &my_addr, sizeof(my_addr)) < 0) {
			close(sock);

			if (errno == EINTR) {
				dprintf(fd, "%s.%03li - Interrupting from bind() syscall\n", cur_time, msec);
				break;
			}

			msec = time_formatted(cur_time);
			dprintf(fd, "%s.%03li - Unable to bind a socket: %s. Waiting for %d seconds\n", cur_time, msec, strerror(errno), NETWORK_UNAVAILABLE_WAIT);
			new_sleep(NETWORK_UNAVAILABLE_WAIT);
			continue;
		}

		char buffer[1000];
		char printable_buffer[sizeof(buffer)];
		struct sockaddr incoming_addr;
		struct sockaddr_in *in_ptr = (void *) &incoming_addr;
		socklen_t incoming_addr_len[1] = {sizeof(incoming_addr)};
		start_over:
		;
		ssize_t got = recvfrom(sock, buffer, sizeof(buffer), 0, &incoming_addr, incoming_addr_len);
		msec = time_formatted(cur_time);
		if (got < 0) {
			close(sock);

			if (errno == EINTR) {
				dprintf(fd, "%s.%03li - Interrupting from recvfrom() syscall\n", cur_time, msec);
				break;
			}

			dprintf(fd, "%s.%03li - Network error: %s. Waiting for %d seconds\n", cur_time, msec, strerror(errno), NETWORK_UNAVAILABLE_WAIT);
			new_sleep(NETWORK_UNAVAILABLE_WAIT);
			break;
		}

		size_t printable_chars = buf_to_print(buffer, (size_t) got, printable_buffer);
		printable_buffer[printable_chars] = '\0';

		dprintf(fd, "%s.%03li - Received packet. Payload len: %zi. From: %s. Printable data: %s\n", cur_time, msec, got, inet_ntoa(in_ptr->sin_addr), printable_buffer);

		goto start_over;
	}
}

static void signal_handler(int signo) {}

int main() {
	siginterrupt(SIGINT, 1);
	siginterrupt(SIGTERM, 1);
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	const char file_name[] = "upd_log";
	int fd = open("file_name", O_RDWR | O_CREAT | O_APPEND, DEFAULT_FILE_MODE);
	if (fd < 0) {
		perror(file_name);
		return EXIT_FAILURE;
	}

	worker(fd);

	return EXIT_SUCCESS;
}
