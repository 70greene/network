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

	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(sockfd, &fds);
	int maxfd = sockfd;

	while (1) {
		fd_set s = fds;	
		select(maxfd + 1, &s, NULL, NULL, NULL); 
		if (FD_ISSET(sockfd, &s)) {
			struct sockaddr_in clientaddr;
			socklen_t len;
			int clientfd = accept(sockfd, (struct sockaddr*)&clientaddr, &len);
			printf("Accepted: %d\n", clientfd);

			FD_SET(clientfd, &fds);
			maxfd = clientfd;
		}

		for (int i = sockfd + 1; i <= maxfd; i++) {
			if (FD_ISSET(i, &s)) {
				char buffer[128] = {0};
				int count = recv(i, buffer, 128, 0);
				if (count == 0) {
					printf("Disconnected: %d\n", i);
					FD_CLR(i, &fds);
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