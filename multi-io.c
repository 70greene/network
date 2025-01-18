#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/epoll.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

// ./wrk -c 100 -d 10s -t 50 http://192.168.0.111:2048

#define BUFFER_LENGTH 512

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

	char resource[BUFFER_LENGTH];  // endpoint 

	union {
		RCALLBACK accept_callback;
		RCALLBACK recv_callback;
	} recv_t;
	RCALLBACK send_callback;
};

struct conn_item connlist[1024 * 1024] = {0};

int epfd = 0;

typedef struct conn_item connection_t;

int http_request(connection_t *conn) {
	return 0;
}

int http_response(connection_t *conn) {
	int filefd = open("index.html", O_RDONLY);
	struct stat stat_buf;
	fstat(filefd, &stat_buf);
	conn->wlen = sprintf(conn->wbuffer, 
		"HTTP/1.1 200 OK\r\n"
		"Accept-Ranges: bytes\r\n"
		"Content-Length: %ld\r\n"
		"Content-Type: text/html\r\n"
		"Date: Sat, 06 Aug 2023 13:16:46 GMT\r\n\r\n", stat_buf.st_size);
	int count = read(filefd, conn->wbuffer + conn->wlen, BUFFER_LENGTH-conn->wlen);
	conn->wlen += count;

	return conn->wlen;
}

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

struct timeval start_time;
#define TIME_SUB_MS(tv1, tv2)  ((tv1.tv_sec - tv2.tv_sec) * 1000 + (tv1.tv_usec - tv2.tv_usec) / 1000)

int accept_cb(int fd) {
	struct sockaddr_in clientaddr;
	socklen_t len;
	int clientfd = accept(fd, (struct sockaddr*)&clientaddr, &len);
	// printf("Accepted %d\n", clientfd);

	set_event(clientfd, EPOLLIN, 1);
	connlist[clientfd].fd = clientfd;
	memset(connlist[clientfd].rbuffer, 0, BUFFER_LENGTH);
	connlist[clientfd].rlen = 0;
	memset(connlist[clientfd].wbuffer, 0, BUFFER_LENGTH);
	connlist[clientfd].wlen = 0;

	connlist[clientfd].recv_t.recv_callback = recv_cb;
	connlist[clientfd].send_callback = send_cb;

	if ((clientfd % 1000) == 999) {
		struct timeval cur_time;
		gettimeofday(&cur_time, NULL);
		int time_used = TIME_SUB_MS(tv_cur, zvoice_king);
		memcpy(&start_time, &cur_time, sizeof(struct timeval));
		printf("clientfd : %d, time_used: %d\n", clientfd, time_used);
	}

	return clientfd;
}

int recv_cb(int fd) {
	char *rbuffer = connlist[fd].rbuffer;
	int idx = connlist[fd].rlen;
	int count = recv(fd, rbuffer + idx, BUFFER_LENGTH - idx, 0);
	if (count == 0) {
		// printf("Disconnected %d\n", fd);
		epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);		
		close(fd);
		return -1;
	}
	connlist[fd].rlen += count;

	http_request(&connlist[fd]);
	http_response(&connlist[fd]);

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

int init_server(unsigned short port) {
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(struct sockaddr_in));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(port);
	bind(sockfd, (struct sockaddr *)&serveraddr, sizeof(struct sockaddr));
	listen(sockfd, 10);
	return sockfd;
}

int main() {
	int port_count = 20;
	unsigned short port = 2048;
	epfd = epoll_create(1);
	for (int i = 0; i < port_count; i++) {
		int sockfd = init_server(port + i);
		connlist[sockfd].fd = sockfd;
		connlist[sockfd].recv_t.accept_callback = accept_cb;
		set_event(sockfd, EPOLLIN, 1);
	}
	gettimeofday(&start_time, NULL);

	struct epoll_event events[1024] = {0};
	while (1) {
		int nready = epoll_wait(epfd, events, 1024, -1);
		for (int i = 0; i < nready; i++) {
			int connfd = events[i].data.fd;
			if (events[i].events & EPOLLIN) {
				int count = connlist[connfd].recv_t.recv_callback(connfd);
				// printf("%d received, count: %d <-- buffer: %s\n", connfd, count, connlist[connfd].rbuffer);
			} else if (events[i].events & EPOLLOUT) {
				connlist[connfd].send_callback(connfd);
				// printf("%d sent--> buffer: %s\n",  connfd, connlist[connfd].wbuffer);
			}
		}
	}

	return 0;
}