#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/poll.h>

int main() {
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(struct sockaddr_in));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(2048);
	bind(sockfd, (struct sockaddr *)&serveraddr, sizeof(struct sockaddr));
	listen(sockfd, 10);

	struct pollfd fds[1024] = {0};
	fds[sockfd].fd = sockfd;
	fds[sockfd].events = POLLIN;
	int maxfd = sockfd;

	while (1) {
		poll(fds, maxfd + 1, -1);
		if (fds[sockfd].revents & POLLIN) {
			struct sockaddr_in clientaddr;
			socklen_t len;
			int clientfd = accept(sockfd, (struct sockaddr*)&clientaddr, &len);
			printf("Accepted: %d\n", clientfd);

			fds[clientfd].fd = clientfd;
			fds[clientfd].events = POLLIN;
			maxfd = clientfd;
		} 

		for (int i = sockfd + 1; i <= maxfd; i++) {
			if (fds[i].revents & POLLIN) {
				char buffer[128] = {0};
				int count = recv(i, buffer, 128, 0);
				if (count == 0) {
					printf("Disconnected %d\n", i);
					fds[i].fd = -1;
					fds[i].events = 0;
					close(i);
					continue;
				}
				
				send(i, buffer, count, 0);
				printf("clientfd: %d, count: %d, buffer: %s\n", i, count, buffer);
			}
		}
	}

	close(sockfd);
	return 0;
}