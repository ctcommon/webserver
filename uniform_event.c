/*信号处理函数需要尽可能快的执行完毕，以确保该信号不被屏蔽（为了避免
 * 一些竟态条件，信号在处理期间，系统不会再次触发它） 太久。典型的解决
 * 是：把信号的主要处理逻辑放到程序的主循环中，信号处理函数只是简单的
 * 通知主循环程序接收到信号并把信号值传递给主循环。准循环再根据接收到的
 * 信号值执行目标信号对应的逻辑代码。信号处理函数通常使用管道将信号传递
 * 给主循环，为了让主循环知道管道上何时有数据可读，需要用到I/O复用系统
 * 调用来监听管道的读端文件描述符上的可读事件。如此一来，信号事件就和其
 * 它I/O事件一样被处理，这就是统一事件源*/
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <pthread.h>

#define MAX_EVENT_NUMBER 1024
static int pipefd[2];

int setnonblocking(int fd)
{
	int old_option = fcntl(fd,F_GETFL);
	int new_option = old_option | O_NONBLOCK;
	fcntl(fd,F_SETFL,new_option);
	return old_option;
}

void addfd(int epollfd, int fd)
{
	epoll_event event;
	event.data.fd = fd;
	event.events = EPOLLIN | EPOLLET;
	setnonblocking(fd);
	epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
}

/*信号处理函数*/
void sig_handler(int sig)
{
	/*保留原来的errno,在函数最后恢复，以保证函数的可重入性*/
	int save_errno = errno;
	int msg = sig;
	send(pipefd[1],(char*)&msg,1,0); /*将信号写入管道，以通知主循环*/
	errno = save_errno;
}
/*设置信号的处理函数*/
void addsig(int sig)
{
	struct sigaction sa;
	memset(&sa,'\0',sizeof(sa));
	sa.sa_handler = sig_handler;
	sa.sa_flags |= SA_RESTART;
	sigfillset(&sa.sa_mask);
	assert(sigaction(sig,&sa,NULL) != -1);
}

int main(int argc, char* argv[])
{
	if(argc < 2)
	{
		printf("usage: %s ip_address port_number\n",basename(argv[0]));
		return 1;
	}
	const char* ip = argv[1];
	int port = atoi(argv[2]);

	struct sockaddr_in address;
	bzero(&address,sizeof(address));
	address.sin_family = AF_INET;
	inet_pton(AF_INET,ip,&address.sin_addr);
	address.sin_port = htons(port);

	int listenfd = socket(PF_INET,SOCK_STREAM,0);
	assert(listenfd >= 0);

	int ret = bind(listenfd,(struct sockaddr*)&address,sizeof(address));
	if(ret == -1)
	{
		printf("errno is %d\n",errno);
		return 1;
	}
	ret = listen(listenfd,5);
	assert(ret != -1);

	epoll_event events[MAX_EVENT_NUMBER];
	int epollfd = epoll_create(5);
	assert(epollfd != -1);
	addfd(epollfd,listenfd);

	/*使用socketpair创建管道，注册pipefd[0]上的可读事件*/
	ret = socketpair(PF_INET,SOCK_STREAM,0,pipefd);
	assert(ret != -1);
	setnonblocking(pipefd[1]);
	addfd(epollfd,pipefd[0]);

	/*设置一些信号的处理函数*/
	addsig(SIGHUP);
	addsig(SIGCHLD);
	addsig(SIGTERM);
	addsig(SIGINT);
	bool stop_server = false;

	while(!stop_server)
	{
		int number = epoll_wait(epollfd,events,MAX_EVENT_NUMBER,-1);
		if((number < 0) && (errno != EINTR))
		{
			printf("epoll failure\n");
			break;
		}

		for(int i=0; i<number; i++)
		{
			int sockfd = events[i].data.fd;
			/*如果就绪文件描述符是listenfd，则处理新的连接*/
			if(sockfd == listenfd)
			{
				struct sockaddr_in client_address;
				socklen_t client_addrlength = sizeof(address);
				int connfd = accept(listenfd,(struct sockaddr*)&client_address,
														&client_addrlength);
				addfd(epollfd,connfd);
			}
			/*如果就绪文件描述符是pipefd[0]，则处理信号*/
			else if((sockfd == pipefd[0]) && (events[i].events & EPOLLIN))
			{
				int sig;
				char signals[1024];
				ret = recv(pipefd[0],signals,sizeof(signals),0);
				if(ret == -1)
				{
					continue;
				}
				else if(ret == 0)
				{
					continue;
				}
				else
				{
					/*因为每个信号值占1字节所以逐字节接收信号，我们以SIGTERM为例，
					 * 来说明如何安全地终止服务器主循环*/
					for(int i=0; i<ret; i++)
					{
						switch(signals[i])
						{
							case SIGCHLD:
							case SIGHUP:
								continue;
							case SIGTERM:
							case SIGINT:
								stop_server = true;
						}
					}
				}
			}
			else
			{
			}
		}
	}
	printf("close fds\n");
	close(listenfd);
	close(pipefd[1]);
	close(pipefd[0]);
	return 0;
}
