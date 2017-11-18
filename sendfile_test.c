#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sendfile.h>

//sendfile函数几乎是专门为在网络上传输文件而设计的，它的优势是
//在两个文件描述符之间传递数据的操作完全是在内核当中，从而
//避免了缓冲区和用户缓冲区之间的数据拷贝，效率很高（实现了零拷贝）

/*ssize_t sendfile(int out_fd, int in_fd, off_t* offset, size_t count)
 * 文件描述符in_fd参数用于从文件读出内容，因此必须是一个支持mmap函数的
 * 文件描述符，即其必须指向真实的文件，不能是socket和管道。
 * 文件描述符out_fd参数用于写入内容，必须是一个socket供数据写入。*/

int main(int argc, char *argv[])
{
	if(argc <= 3)
	{
		printf("usage: %s ip_address port_number filename\n",basename(argv[0]));
		return 1;
	}
	const char* ip = argv[1];
	int port = atoi(argv[2]);
	const char* file_name = argv[3];

	int filefd = open(file_name, O_RDONLY);
	assert(filefd > 0);
	struct stat stat_buf;
	fstat(filefd, &stat_buf);

	struct sockaddr_in address;
	bzero(&address,sizeof(address));
	address.sin_family = AF_INET;
	inet_pton(AF_INET,ip,&address.sin_addr);
	address.sin_port = htons(port);

	int sock = socket(PF_INET,SOCK_STREAM,0);
	assert(sock >= 0);

	int ret = bind(sock,(struct sockaddr*)&address,sizeof(address));
	assert(ret != -1);

	ret = listen(sock,5);
	assert(ret != -1);

	struct sockaddr_in client;
	socklen_t client_addrlength = sizeof(client);
	int connfd = accept(sock,(struct sockaddr*)&client, &client_addrlength);
	if(connfd < 0)
		printf("errno is: %d\n",errno);
	else
	{
		sendfile(connfd,filefd,NULL,stat_buf.st_size);
		close(connfd);
	}
	close(sock);
	return 0;
}
