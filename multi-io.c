#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/epoll.h>

#define BUFFER_LENGTH 1024

struct conn_item {
	int idx;
	char buffer[BUFFER_LENGTH];
};
struct conn_item connlist[1024] = {0};

int main() {
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(struct sockaddr_in));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(2048);
	bind(sockfd, (struct sockaddr *)&serveraddr, sizeof(struct sockaddr));
	listen(sockfd, 10);

	int epfd = epoll_create(1);  // 4
	struct epoll_event ev;
	ev.data.fd = sockfd;
	ev.events = EPOLLIN;
	epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev);
	struct epoll_event events[1024] = {0};
	while (1) {
		int nready = epoll_wait(epfd, events, 1024, -1);
		for (int i = 0; i < nready; i++) {
			int connfd = events[i].data.fd;
			if (sockfd == connfd) {
				struct sockaddr_in clientaddr;
				socklen_t len;
				int clientfd = accept(sockfd, (struct sockaddr*)&clientaddr, &len);
				ev.data.fd = clientfd;
				ev.events = EPOLLIN | EPOLLET;
				epoll_ctl(epfd, EPOLL_CTL_ADD, clientfd, &ev);
				printf("Accepted: %d\n", clientfd);
			} else if (events[i].events & EPOLLIN) {
				int idx = connlist[connfd].idx;
				char *buffer = connlist[connfd].buffer;
				int count = recv(connfd, buffer + idx, BUFFER_LENGTH - idx, 0);
				if (count == 0) {
					printf("Disconnected %d\n", connfd);
					epoll_ctl(epfd, EPOLL_CTL_DEL, connfd, NULL);
					close(i);
					continue;
				}
				connlist[connfd].idx += count;
				
				send(connfd, buffer, connlist[connfd].idx, 0);
				printf("clientfd: %d, count: %d, buffer: %s\n", connfd, connlist[connfd].idx, buffer);
			}
		}
	}

	close(sockfd);
	return 0;
}