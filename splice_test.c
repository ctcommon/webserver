#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

/*函数原型：
 * #include <fcntl.h>
 * ssize_t splice(int fd_1, loff_t* off_1, int fd_2, loff_t* off_2,
 * 								size_t len, unsigned int flags)
 * 整体含义是从fd_1文件描述符所绑定的文件或管道或socket读数据，然后
 * 写入fd_2文件描述符所对应的文件或管道或socket。若fd_1是管道文件描述符
 * ，则off_1必须为NULL，若不是管道文件描述符，off_1表示从数据流的何处读
 * ，此时若为NULL，表示从数据的当前偏移位置读。fd_2以off_2的含义
 * 和fd_1以及off_1的含义一样。使用splice函数时，其中至少一个文件描述符为
 * 管道文件描述符。函数调用成功返回移动字节的数据量。也可能返回0，表示
 * 没有数据需要移动，这发生在从管道读取数据但是管道当中没有被写入数据时
 * 函数调用失败返回-1并设置errno*/

int main(int argc, char* argv[])
{
	if(argc <= 2)
	{
		printf("usage: %s ip_address port_number\n",basename(argv[0]));
		return 1;
	}
	const char* ip = argv[1];
	int port = atoi(argv[2]);

	struct sockaddr_in address;
	bzero(&address,sizeof(address));
	address.sin_family = AF_INET;
	inet_pton(AF_INET, ip, &address.sin_addr);
	address.sin_port = htons(port);

	int sock = socket(PF_INET,SOCK_STREAM,0);
	assert(sock >= 0);

	int ret = bind(sock,(struct sockaddr*)&address,sizeof(address));
	assert(ret != -1);

	ret = listen(sock,5);
	assert(ret != -1);

	struct sockaddr_in client;
	socklen_t client_addrlength = sizeof(client);
	int connfd = accept(sock,(struct sockaddr*)&client,&client_addrlength);
	if(connfd < 0)
	{
		printf("errno is %d\n",errno);
	}
	else
	{
		int pipefd[2];
		ret = pipe(pipefd);
		assert(ret != -1);
		//将connfd上流入的客户数据定向到管道中
		ret = splice(connfd,NULL,pipefd[1],NULL,32768,
				SPLICE_F_MORE|SPLICE_F_MOVE);
		assert(ret != -1);
		//将管道的输出定向到connfd客户连接文件描述符
		ret = splice(pipefd[0],NULL,connfd,NULL,32768,
				SPLICE_F_MORE|SPLICE_F_MOVE);
		assert(ret != -1);
		close(connfd);
	}
	close(sock);
	return 0;
}
