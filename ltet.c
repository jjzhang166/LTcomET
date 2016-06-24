#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/types.h>      
#include <sys/socket.h>
#include <stdbool.h>

#define _BUF_SIZE_ 10
#define _EVENT_NUMS_ 64

//将文件描述符设置为非阻塞的
int setnonblock(int fd)
{
	int old_option=fcntl(fd,F_GETFL);
	int new_option=old_option|O_NONBLOCK;
	fcntl(fd,F_SETFL,new_option);
	return old_option;
}

//将文件描述符fd上的EPOLLIN注册到epolled指示的epoll内核事件表中，采纳数enable——et指定是否对fd启用et模式
void addfd(int epollfd,int fd,bool dou)
{
	struct epoll_event ev;
	ev.events=EPOLLIN;
	ev.data.fd=fd;
	if(dou)
	{
		ev.events|=EPOLLET;
	}
	epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&ev);
	setnonblock(fd);
}

//lt模式的工作流程
void lt(struct epoll_event* events,int number,int epollfd,int listenfd)
{
	char buf[_BUF_SIZE_];
	int i=0;
	for(i=0;i<number;i++)
	{
		int sockfd=events[i].data.fd;
		if(sockfd==listenfd)
		{
			struct sockaddr_in client;
			socklen_t len =sizeof(client);
			int connfd=accept(listenfd,(struct sockaddr*)&client,&len);
			addfd(epollfd,connfd,false);
		}
		else if(events[i].events&EPOLLIN)
		{
			//只要sock读缓存中还有未读出的数据，这段代码就被触发i
			printf("###################ltlt################\n");
			memset(buf,'\0',_BUF_SIZE_);
			int ret=recv(sockfd,buf,_BUF_SIZE_-1,0);
			if(ret<=0)
			{
				close(sockfd);
				continue;
			}
			printf("get %d bytes of content:%s\n",ret,buf);
		}
		else{
			printf("something else happend\n");
		}
	}
}

void et(struct epoll_event* events,int number,int epollfd,int listenfd)
{
	char buf[_BUF_SIZE_];
	int i=0;
	for (i=0;i<number;i++)
	{
		int sockfd=events[i].data.fd;
		if(sockfd==listenfd)
		{
			struct sockaddr_in client;
			socklen_t len=sizeof(client);
			int connfd=accept(listenfd,(struct sockaddr*)&client,&len);
			addfd(epollfd,connfd,true);
		}
		else if(events[i].events&EPOLLIN)
		{
			printf("**************etetet*********************\n");
			//这段代码不会被重复触发，所以我们循环读取数据，以确保把socket读缓存中的所有数据读出
			while(1)
			{
				memset(buf,'\0',_BUF_SIZE_);
				int ret=recv(sockfd,buf,_BUF_SIZE_-1,0);
				if(ret<0)
				{
					//对于非阻塞IO，下面的条件成立表示数据已经全部读取完毕，此后，epoll就能再次触发sockfd上的EPOLLIN事件，以驱动下一次读操作
					if((errno==EAGAIN)||(errno==EWOULDBLOCK))
					{
						printf("read later\n");
						break;
					}
					close(sockfd);
					break;
				}
				else if(ret==0)
				{
					close(sockfd);
				}
				else{
					printf("get %d bytes of content:%s\n",ret,buf);
				}

			}
		}
		else
		{
			printf("something else happen\n");
		}
	}
}

int main(int argc,char *argv[])
{
	if(argc<3)
	{
		printf("usage:%s [ip] [port]\n");
	}

	char *ip=argv[1];
	int port=atoi(argv[2]);

	struct sockaddr_in local;
	bzero(&local,sizeof(local));
	local.sin_family=AF_INET;
	local.sin_addr.s_addr=inet_addr(ip);
	local.sin_port=htons(port);

	int ret;
	int sock=socket(AF_INET,SOCK_STREAM,0);
	assert(sock>=0);

	int opt=1;
	setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));

	ret=bind(sock,(struct sockaddr*)&local,sizeof(local));
	assert(ret=-1);

	ret=listen(sock,5);
	assert(ret=-1);

	struct epoll_event events[_EVENT_NUMS_];
	int epollfd=epoll_create(256);
	assert(epollfd!=-1);
	addfd(epollfd,sock,true);
	int timeout=5000;

	while(1)
	{
		int ret=epoll_wait(epollfd,events,_EVENT_NUMS_,timeout);
		if(ret<0)
		{
			printf("epoll_wait faild\n");
			break;
		}
		else if(ret==0)
		{
			printf("timeout\n");
			continue;
		}
	//	lt(events,ret,epollfd,sock);
		et(events,ret,epollfd,sock);
	}
	close(sock);
	return 0;
}
