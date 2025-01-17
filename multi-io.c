#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/epoll.h>

#define BUFFER_LENGTH 1024

typedef int (*RCALLBACK)(int fd);
int accept_cb(int fd);
int recv_cb(int fd);
int send_cb(int fd);

struct conn_item {
	int fd;
	char rbuffer[BUFFER_LENGTH];
	int rlen;
	char wbuffer[BUFFER_LENGTH];
	int wlen;

	union {
		RCALLBACK accept_callback;
		RCALLBACK recv_callback;
	} recv_t;
	RCALLBACK send_callback;
};

struct conn_item connlist[1024] = {0};

int epfd = 0;

int set_event(int fd, int event, int flag) {
	struct epoll_event ev;
	ev.data.fd = fd;
	ev.events = event;

	if (flag) {  // 1 add, 0 mod
		epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
	} else {
		epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
	}
}

int accept_cb(int fd) {
	struct sockaddr_in clientaddr;
	socklen_t len;
	int clientfd = accept(fd, (struct sockaddr*)&clientaddr, &len);
	printf("Accepted %d\n", clientfd);

	set_event(clientfd, EPOLLIN, 1);
	connlist[clientfd].fd = clientfd;
	memset(connlist[clientfd].rbuffer, 0, BUFFER_LENGTH);
	connlist[clientfd].rlen = 0;
	memset(connlist[clientfd].wbuffer, 0, BUFFER_LENGTH);
	connlist[clientfd].wlen = 0;

	connlist[clientfd].recv_t.recv_callback = recv_cb;
	connlist[clientfd].send_callback = send_cb;

	return clientfd;
}

int recv_cb(int fd) {
	char *rbuffer = connlist[fd].rbuffer;
	int idx = connlist[fd].rlen;
	int count = recv(fd, rbuffer + idx, BUFFER_LENGTH - idx, 0);
	if (count == 0) {
		printf("Disconnected %d\n", fd);
		epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);		
		close(fd);
		return -1;
	}
	connlist[fd].rlen += count;

	memcpy(connlist[fd].wbuffer, connlist[fd].rbuffer, connlist[fd].rlen);
	connlist[fd].wlen = connlist[fd].rlen;
	connlist[fd].rlen = 0;

	set_event(fd, EPOLLOUT, 0);
	return count;
}

int send_cb(int fd) {
	char *wbuffer = connlist[fd].wbuffer;
	int idx = connlist[fd].wlen;
	int count = send(fd, wbuffer, idx, 0);
	set_event(fd, EPOLLIN, 0);
	return count;
}

int main() {
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(struct sockaddr_in));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(2048);
	bind(sockfd, (struct sockaddr *)&serveraddr, sizeof(struct sockaddr));
	listen(sockfd, 10);

	connlist[sockfd].fd = sockfd;
	connlist[sockfd].recv_t.accept_callback = accept_cb;

	epfd = epoll_create(1);
	set_event(sockfd, EPOLLIN, 1);

	struct epoll_event events[1024] = {0};
	while (1) {
		int nready = epoll_wait(epfd, events, 1024, -1);
		for (int i = 0; i < nready; i++) {
			int connfd = events[i].data.fd;
			if (events[i].events & EPOLLIN) {
				int count = connlist[connfd].recv_t.recv_callback(connfd);
				printf("%d received, count: %d <-- buffer: %s\n", connfd, count, connlist[connfd].rbuffer);
			} else if (events[i].events & EPOLLOUT) {
				connlist[connfd].send_callback(connfd);
				printf("%d sent--> buffer: %s\n",  connfd, connlist[connfd].wbuffer);
			}
		}
	}

	close(sockfd);
	return 0;
}