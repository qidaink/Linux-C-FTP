/** =====================================================
 * Copyright © hk. 2022-2025. All rights reserved.
 * File name: serverFTP.c
 * Author   : qidaink
 * Date     : 2022-07-10
 * Version  :
 * Description:
 * Others   :
 * Log      :
 * ======================================================
 */
/* 头文件 */
#include <stdio.h>      /* perror scanf printf fopen fwrite */
#include <stdlib.h>     /* exit   atoi  system */
#include <unistd.h>     /* sleep  read  close getpid fork stat getcwd chdir access */
#include <errno.h>      /* errno号 */

#include <sys/types.h>  /* socket getpid    bind listen accept send fork stat mkdir open */
#include <sys/socket.h> /* socket inet_addr bind listen accept send */
#include <arpa/inet.h>  /* htons  inet_addr inet_pton */
#include <netinet/in.h> /* ntohs  inet_addr inet_ntop*/

#include <string.h>     /* bzero strncasecmp strlen memset */

#include <signal.h>     /* signal */
#include <sys/wait.h>   /* waitpid */

#include <dirent.h>     /* opendir readdir*/
#include <sys/stat.h>   /* stat mkdir open*/
#include <time.h>       /* localtime localtime_r */

#include <fcntl.h>      /* open */


/* printf打印输出的颜色定义 */
/* 前景色(字体颜色) */
#define CLS           "\033[0m"    /* 清除所有颜色 */
#define BLACK         "\033[1;30m" /* 黑色加粗字体 */
#define RED           "\033[1;31m" /* 红色加粗字体 */
#define GREEN         "\033[1;32m" /* 绿色加粗字体 */
#define YELLOW        "\033[1;33m" /* 黄色加粗字体 */
#define BLUE          "\033[1;34m" /* 蓝色加粗字体 */
#define PURPLE        "\033[1;35m" /* 紫色加粗字体 */
#define CYAN          "\033[1;36m" /* 青色加粗字体 */
#define WHITE         "\033[1;37m" /* 白色加粗字体 */
#define BOLD          "\033[1m"    /* 加粗字体 */

/* 命令的类型 */
#define CONNECT_INFO 0 /* 客户端请求服务器回传客户端IP和端口号 */
#define SERVER_PWD   1 /* 显示服务器当前路径 */
#define SERVER_LS    2 /* 显示服务器当前路径所有文件信息 */
#define SERVER_MKDIR 3 /* 在服务器当前路径下新建目录 */
#define SERVER_RM    4 /* 在服务器当前路径下删除一个文件或者目录 */
#define SERVER_CD    5 /* 在服务器下切换目录 */
#define PUT_FILE     6 /* 客户端上传文件 */
#define GET_FILE     7 /* 客户端下载文件 */

/* 客户端信息的结构体 */
typedef struct
{
	char ipv4_addr[16];/* 转换后的IP地址 */
	int port;          /* 转换后的端口 */
} CLIENT_INFO;

/* 定义通信双方的信息结构体 */
#define N 32
typedef struct
{
	int flag;/* 标记是否为root用户 */
	int type;/* 命令类型 */
	char name[N];  /* 用户名 */
	char data[256];/* 数据 */
	CLIENT_INFO client_info[2];/* 存放客户端和服务器端信息 */
	int result;/* 标记是否操作成功，-1,失败;0,默认状态;1,成功 */
} MSG;


/* 服务器启动 */
int serverStartInfo(void);/* 服务器启动菜单 */
int serverStart(void);/* 启动服务器 */
int serverEnterListen(char *serverIP, int serverPort);/* 服务器进入监听状态 */
/* 客户端数据处理(多进程) */
int clientDataHandle(void *arg, CLIENT_INFO clientInfo);/* 进程处理客户端数据函数 */
void sigChildHandle(int signo);  /* 信号回收子进程，避免出现僵尸进程 */
/* 客户端数据处理功能实现 */
int connectInfoHandle(int accept_fd, MSG * msg);     /* 处理服务器返回客户端IP和端口号的请求 */
int serverLocalPWDHandle(int accept_fd, MSG * msg);  /* 处理客户端获取服务器当前路径的请求 */
int getFileInfo(const char * filename, char * data); /* 获取指定文件信息 */
int serverFileListHandle(int accept_fd, MSG * msg);  /* 处理客户端获取服务器当前路径所有文件的请求 */
int createServerDirHandle(int accept_fd, MSG * msg); /* 处理客户端获取服务器当前路径新建目录的请求 */
int deleteServerFileHandle(int accept_fd, MSG * msg);/* 在服务器当前路径下删除文件或者目录请求处理 */
int cdServerDirHandle(int accept_fd, MSG * msg);     /* 在服务器端切换路径 */
int clientPutFileHandle(int socket_fd, MSG * msg);   /* 客户端上传文件到服务器请求处理 */
int clientGetFileHandle(int accept_fd, MSG * msg);   /* 客户端从服务器下载文件 */

int main(int argc, char *argv[])
{
	int socket_fd = -1;/* 服务器的socket套接字 */
	/* 1.启动服务器 */
	if ( (socket_fd = serverStart()) < 0)
	{
		printf(RED"[error]server error!\n"CLS);
		return -1;
	}
	/* 2.注册信号处理函数，实现进程结束自动回收 */
	signal(SIGCHLD, sigChildHandle);
	/* 4.阻塞等待客户端连接请求所需变量定义 */
	int accept_fd = -1;
	struct sockaddr_in cin;          /* 用于存储成功连接的客户端的IP信息 */
	socklen_t addrlen = sizeof(cin);
	/* 5.打印成功连接的客户端的信息相关变量定义 */
	CLIENT_INFO clientInfo;
	/* 6.多进程程相关变量定义 */
	pid_t pid = -1;
	while(1)
	{
		/* 等待连接 */
		if ((accept_fd = accept(socket_fd, (struct sockaddr *)&cin, &addrlen)) < 0)
		{
			perror(RED"[error]accept"CLS);
			exit(-1);
		}
		/* 创建进程处理客户端请求 */
		pid = fork();
		if(pid < 0)/* 进程创建失败 */
		{
			perror(RED"[error]fork"CLS);
			break;
		}
		else if(pid > 0)/* 父进程 */
		{
			/* 关闭正文件描述符的话可以节约资源，但是这样每个客户端连接过来的描述符都是一样的，但是通信并不受影响 */
			// close(accept_fd);/* 子进程会复制父进程的资源包括文件描述符，所以父进程这里不再需要这个新的socket描述符 */
		}
		else /* 子进程，这里会复制之前成功连接到服务器的客户端而产生的的新的accept_fd */
		{
			close(socket_fd);/* 子进程用于处理客户端请求，所以不需要用于监听的那个socket套接字了 */
			/* 获取连接成功的客户端信息 */
			if (!inet_ntop(AF_INET, (void *)&cin.sin_addr, clientInfo.ipv4_addr, sizeof(cin)))
			{
				perror(RED"[error]inet_ntop"CLS);
				exit(-1);
			}
			clientInfo.port = ntohs(cin.sin_port);
			clientDataHandle(&accept_fd, clientInfo);/* 处理客户端数据 */
			exit(0);
		}
	}
	/* 7.关闭文件描述符 */
	close(socket_fd);
	return 0;
}
/*=================================================*/
/**
 * @Function: serverStartInfo
 * @Description: 服务器启动菜单
 * @param   : none
 * @return  : 返回一个整数
 *            0,显示成功;
 *            -1,显示失败
 */
int serverStartInfo(void)
{
	printf("\n\033[1m---------------- Server Start Description -------------------\033[0m\n");
	printf("|\033[1;32m First, We should enter the IP and port of the server.\033[0m\n");
	printf("|\n");
	printf("|\033[1;33m serverIP: IP address of the local NIC(or 0.0.0.0).\033[0m\n");
	printf("|\n");
	printf("|\033[1;33m serverPort: >5000.\033[0m\n");
	printf("|\033[1;31m Attention: serverIP = 0.0.0.0 , It listens to all IP addresses.\033[0m\n");
	printf("\033[1m-----------------------------------------------------------\033[0m\n");
	return 0;
}

/**
 * @Function: serverStart
 * @Description: 启动服务器
 * @param   : none
 * @return  : 返回一个整数
 *            非负数,启动成功，且表示socket描述符;
 *            -1,启动失败
 */
int serverStart(void)
{
	char confirm;
	int ret = -1;
	char serverIP[16] = {0}; /* 服务器端IP地址 */
	int serverPort = -1; /* 服务器端口 */
	/* 1.打印提示信息 */
	serverStartInfo();
	printf("Whether to use the default settings(Y or N):");
	ret = scanf("%c%*[^\n]", &confirm);
	if(ret < 0)
	{
		perror(RED"[error]scanf"CLS);
		return -1;
	}
	if(confirm == 'Y' || confirm == 'y')
	{
		strncpy(serverIP, "0.0.0.0", 16);
		serverPort = 5001;
	}
	else
	{
		/* 2.输入服务器端IP地址和端口 */
		printf("serverIP serverPort: ");
		ret = scanf("%s %d%*[^\n]", serverIP, &serverPort);
		if(ret < 0)
		{
			perror(RED"[error]scanf"CLS);
			return -1;
		}
	}
	printf(GREEN"[The entered data]:\n"CLS);
	printf("server:"GREEN" %s %d\n"CLS, serverIP, serverPort);
	/* 3.建立TCP连接 */
	ret = serverEnterListen(serverIP, serverPort);
	return ret;
}

/**
 * @Function: serverEnterListen
 * @Description: 服务器进入监听状态
 * @param serverIP   : 服务器IP
 * @param serverPort : 服务器端口
 * @return  : 返回一个整数
 *            socket_fd,启动成功，并返回socket套接字;
 *            -1,启动失败
 */
int serverEnterListen(char *serverIP, int serverPort)
{
	/* 1.创建套接字，得到套接字描述符 */
	int socket_fd = -1; /* 接收服务器端socket描述符 */
	if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror(RED"[error]socket"CLS);
		exit(-1);
	}
	/* 2.网络属性设置 */
	/* 2.1允许绑定地址快速重用 */
	int b_reuse = 1;
	setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &b_reuse, sizeof (int));
	/* 3.服务器绑定IP和端口 */
	/* 3.1填充struct sockaddr_in结构体变量 */
	struct sockaddr_in sin;
	bzero (&sin, sizeof(sin)); /* 将内存块（字符串）的前n个字节清零 */
	sin.sin_family = AF_INET;   /* 协议族 */
	sin.sin_port = htons(serverPort); /* 网络字节序的端口号 */
	if(inet_pton(AF_INET, serverIP, (void *)&sin.sin_addr) != 1)/* IP地址 */
	{
		perror(RED"[error]inet_pton"CLS);
		return -1;
	}
	/* 3.2绑定 */
	if(bind(socket_fd, (struct sockaddr *)&sin, sizeof(sin)) < 0)
	{
		perror(RED"[error]bind"CLS);
		return -1;
	}
	/*4.调用listen()把主动套接字变成被动套接字 */
	if (listen(socket_fd, 5) < 0)
	{
		perror(RED"[error]listen"CLS);
		return -1;
	}
	printf("server"GREEN"[%s:%d]"CLS" staring...OK![socket_fd=%d]\n", serverIP, serverPort, socket_fd);
	return socket_fd;
}

/*=================================================*/
/**
 * @Function: clientDataHandle
 * @Description: 子进程处理客户端数据函数
 * @param arg : 得到客户端连接产生新的套接字文件描述符
 * @param clientInfo : 客户端IP和端口信息
 * @return  : 返回一个整数
 *            0,数据处理完毕;
 */
int clientDataHandle(void *arg, CLIENT_INFO clientInfo)
{
	int accept_fd = *(int *) arg; /* 表示客户端通信的新的socket fd */
	MSG msg; /* 与客户端通信的数据信息结构体变量 */
	printf("clinet"GREEN"[%s:%d]"CLS"is connected successfully![accept_fd=%d]\n", clientInfo.ipv4_addr, clientInfo.port, accept_fd);
	printf ("client"GREEN"[%s:%d]"CLS" data handler process(PID:%d).[accept_fd =%d]\n", clientInfo.ipv4_addr, clientInfo.port, getpid(), accept_fd);
	/* 数据读写 */
	while( recv(accept_fd, &msg, sizeof(msg), 0) > 0)
	{
		printf(PURPLE"[client(%s:%d)]:"CLS" msg type:%d(msg type)\n", clientInfo.ipv4_addr, clientInfo.port, msg.type);
		switch(msg.type)
		{
		case CONNECT_INFO:/* 客户端启动，请求获取客户端的IP和端口 */
			connectInfoHandle(accept_fd, &msg);
			break;
		case SERVER_PWD:  /* 显示服务器端当前路径请求处理 */
			serverLocalPWDHandle(accept_fd, &msg);
			break;
		case SERVER_LS:   /* 显示服务器端当前路径下所有文件请求处理 */
			serverFileListHandle(accept_fd, &msg);
			break;
		case SERVER_MKDIR:/* 在服务器当前路径下新建目录请求处理 */
			createServerDirHandle(accept_fd, &msg);
			break;
		case SERVER_RM:   /* 在服务器当前路径下删除文件或者目录请求处理 */
			deleteServerFileHandle(accept_fd, &msg);
			break;
		case SERVER_CD:   /* 在服务器切换路径 */
			cdServerDirHandle(accept_fd, &msg);
			break;
		case PUT_FILE:   /* 客户端上传到服务器 */
			clientPutFileHandle(accept_fd, &msg);
			break;
		case GET_FILE:   /* 客户端上传到服务器 */
			clientGetFileHandle(accept_fd, &msg);
			break;
		default:
			printf("Invalid data msg.\n");
			break;
		}
	}
	printf ("client"GREEN"[%s:%d]"CLS" exit ... [accept_fd =%d]\n", clientInfo.ipv4_addr, clientInfo.port, accept_fd);

	/* 6.关闭文件描述符 */
	close(accept_fd);
	return 0;
}
/*=================================================*/
/**
 * @Function: sigChildHandle
 * @Description: 通过信号回收子进程
 * @param signo: 检测到的信号(子进程结束时会发出SIGCHLD信号)
 * @return  : none
 */
void sigChildHandle(int signo)
{
	if(signo == SIGCHLD)
	{
		waitpid(-1, NULL,  WNOHANG);
	}
}
/**
 * @Function: connectInfoHandle
 * @Description: 处理服务器返回客户端IP和端口号的请求
 * @param accept_fd: 客户端的socket套接字
 * @param msg      : 服务器与客户端通信的数据结构体指针变量
 * @return  : 返回一个整数
 *            0,获取成功;
 *            -1,获取失败
 */
int connectInfoHandle(int accept_fd, MSG * msg)
{
	char ipv4_addr[16] = {}; /* 暂存客户端IP地址 */
	int port = -1;           /* 暂存客户端端口信息 */
	struct sockaddr_in info;  /* 用于存储成功连接的客户端的IP信息 */
	socklen_t addrlen = sizeof(info);
	/* 清空原来的client_info成员数据 */
	bzero(msg->client_info[0].ipv4_addr, sizeof(msg->client_info[0].ipv4_addr));/* 先清空IP地址存储空间信息 */
	bzero(msg->client_info[1].ipv4_addr, sizeof(msg->client_info[1].ipv4_addr));/* 先清空IP地址存储空间信息 */
	msg->result = 0;
	/* 获取客户端信息 */
	if(getpeername(accept_fd, (struct sockaddr *)&info, &addrlen) < 0)
	{
		perror(RED"[error]getsockname"CLS);
		msg->result = -1;/* 表示获取客户端信息失败 */
	}
	if (!inet_ntop(AF_INET, (void *)&info.sin_addr, ipv4_addr, sizeof(info)))
	{
		perror(RED"[error]inet_ntop"CLS);
		msg->result = -1;/* 表示获取客户端信息失败 */
	}
	port = ntohs(info.sin_port);
	/* 获取客户端信息 */
	strncpy(msg->client_info[0].ipv4_addr, ipv4_addr, 16);
	msg->client_info[0].port = port;

	/* 获取服务器端信息 */
	if(getsockname(accept_fd, (struct sockaddr *)&info, &addrlen) < 0)
	{
		perror(RED"[error]getsockname"CLS);
		msg->result = -1;/* 表示获取客户端信息失败 */
	}
	if (!inet_ntop(AF_INET, (void *)&info.sin_addr, ipv4_addr, sizeof(info)))
	{
		perror(RED"[error]inet_ntop"CLS);
		msg->result = -1;/* 表示获取客户端信息失败 */
	}
	port = ntohs(info.sin_port);

	/* 拷贝服务器端信息 */
	strncpy(msg->client_info[1].ipv4_addr, ipv4_addr, 16);
	msg->client_info[1].port = port;


	msg->result = 1;/* 表示获取信息成功 */
	/* 发送回应信息 */
	if( send(accept_fd, msg, sizeof(MSG), 0) < 0)
	{
		perror(RED"[error]send"CLS);
		return -1;
	}
	return 0;
}

/**
 * @Function: serverLocalPWDHandle
 * @Description: 处理客户端获取服务器当前路径的请求
 * @param accept_fd: 客户端的socket套接字
 * @param msg      : 服务器与客户端通信的数据结构体指针变量
 * @return  : 返回一个整数
 *            0,获取成功;
 *            -1,获取失败
 */
int serverLocalPWDHandle(int accept_fd, MSG * msg)
{
	/* 清空原来的data成员数据 */
	bzero(msg->data, sizeof(msg->data)/sizeof(char));/* 先清空IP地址存储空间信息 */
	/* 获取当前路径 */
	if( (getcwd(msg->data, sizeof(msg->data))) == NULL )
	{
		printf(RED"[error]get server local pwd failed!\n"CLS);
	}
	else
	{
		printf(GREEN"[ OK  ]get server local pwd successfully!\n"CLS);
		printf("server local PWD: %s\n", msg->data);
	}

	/* 发送回应信息 */
	if( send(accept_fd, msg, sizeof(MSG), 0) < 0)
	{
		perror(RED"[error]send"CLS);
		return -1;
	}
	return 0;
}

/**
 * @Function: getFileInfo
 * @Description: 获取指定文件信息
 * @param filename: 文件名称(可以包含路径)
 * @param data    : 文件详细信息存储的字符串
 * @return  : 返回一个整数
 *            0,获取成功;
 *            -1,获取失败
 */
// -rwxrwxrwx 2092 2022-5-8 19:47 filename
int getFileInfo(const char * filename, char * data)
{
	/* 定义一个文件属性结构体变量 */
	struct stat file_attr;
	int i = 0;
	struct tm *t;
	char * p = NULL;
	p = data;
	/* 1. 获取文件属性保存到结构体变量中 */
	if( stat(filename, &file_attr) < 0)
	{
		perror(RED"[error]stat"CLS);
		return -1;
	}
	/* 3. 打印文件存取类型 */
	if( S_ISREG(file_attr.st_mode) )
	{
		printf("-");
		sprintf(p++, "-");
	}
	if( S_ISDIR(file_attr.st_mode) )
	{
		printf("d");
		sprintf(p++, "d");
	}
	if( S_ISCHR(file_attr.st_mode) )
	{
		printf("c");
		sprintf(p++, "c");
	}
	if( S_ISBLK(file_attr.st_mode) )
	{
		printf("b");
		sprintf(p++, "b");
	}
	if( S_ISFIFO(file_attr.st_mode) )
	{
		printf("p");
		sprintf(p++, "p");
	}
	if( S_ISSOCK(file_attr.st_mode) )
	{
		printf("s");
		sprintf(p++, "s");
	}
	/* 4. 打印文件权限 */
	for(i = 8; i >= 0; i--)
	{
		if(file_attr.st_mode & (1 << i))
		{
			switch(i % 3)
			{
			case 0:
				printf("x");
				sprintf(p++, "x");
				break;
			case 1:
				printf("w");
				sprintf(p++, "w");
				break;
			case 2:
				printf("r");
				sprintf(p++, "r");
				break;
			}
		}
		else
		{
			printf("-");
			sprintf(p++, "-");
		}
	}
	/* 5. 文件字节数(文件大小) */
	printf(" %5.1fK",(float)file_attr.st_size / 1024);
	sprintf(p, " %5.1fK",(float)file_attr.st_size / 1024);
	while(*(++p) != '\0');
	/* 6. 打印文件修改时间 */
	t = localtime(&file_attr.st_ctime);
	printf(" %d-%02d-%02d %02d:%02d",t->tm_year+1900,t->tm_mon+1,t->tm_mday,t->tm_hour,t->tm_min);
	sprintf(p, " %d-%02d-%02d %02d:%02d",t->tm_year+1900,t->tm_mon+1,t->tm_mday,t->tm_hour,t->tm_min);
	while(*(++p) != '\0');
	if( S_ISDIR(file_attr.st_mode) )
	{
		printf(GREEN" %s\n"CLS, filename);
		sprintf(p, GREEN" %s"CLS, filename);
	}
	else
	{
		printf(" %s\n", filename);
		sprintf(p, " %s", filename);
	}
	return 0;
}

/**
 * @Function: serverFileListHandle
 * @Description: 处理客户端获取服务器当前路径所有文件的请求
 * @param accept_fd: 客户端的socket套接字
 * @param msg      : 服务器与客户端通信的数据结构体指针变量
 * @return  : 返回一个整数
 *            0,获取成功;
 *            -1,获取失败
 */
int serverFileListHandle(int accept_fd, MSG * msg)
{
	DIR * dir;
	struct dirent * dirInfo;
	/* 打开当前路径 */
	if( (dir = opendir("./")) == NULL )
	{
		perror(RED"[error]opendir"CLS);
		return -1;
	}
	/* 先清除标记状态 */
	msg->result = 0;
	/* 打印所有文件名 */
	while( (dirInfo = readdir(dir)) != NULL )
	{
		/* 清空原来的data成员数据 */
		bzero(msg->data, sizeof(msg->data)/sizeof(char));/* 先清空数据字符串空间 */
		/* 跳过 . 和 ..  */
		if(strcmp(dirInfo->d_name, ".") == 0 || strcmp(dirInfo->d_name, "..") == 0)
		{
			continue;
		}
		else
		{
			getFileInfo(dirInfo->d_name, msg->data);/* 此处获取并打印文件详细信息 */
			/* 发送回应信息 */
			if( send(accept_fd, msg, sizeof(MSG), 0) < 0)
			{
				perror(RED"[error]send"CLS);
				return -1;
			}
		}
	}
	/* 清空原来的data成员数据 */
	bzero(msg->data, sizeof(msg->data)/sizeof(char));/* 先清空数据字符串空间 */
	/* 打印当前路径 debug1*/
	if( (getcwd(msg->data, sizeof(msg->data))) == NULL )
	{
		printf(RED"[error]get server local pwd failed!\n"CLS);
	}
	else
	{
		printf(GREEN"[ OK  ]get server local pwd successfully!\n"CLS);
		printf("server local PWD: %s\n", msg->data);
	}
	/* 读取文件结束 */
	msg->result = 1;/* 以此表示文件读取全部结束 */
	if( send(accept_fd, msg, sizeof(MSG), 0) < 0)
	{
		perror(RED"[error]send"CLS);
		return -1;
	}
	closedir(dir);
	return 0;
}

/**
 * @Function: createServerDirHandle
 * @Description: 在服务器端创建目录
 * @param accept_fd: 客户端的socket套接字
 * @param msg      : 服务器与客户端通信的数据结构体指针变量
 * @return  : 返回一个整数
 *            0,创建成功;
 *            -1,创建失败
 */
int createServerDirHandle(int accept_fd, MSG * msg)
{
	char * dir_path;
	msg->result = 0;
	/* 申请内存 */
	if( (dir_path = (char *)malloc(256)) == NULL )/* 申请256字节内存空间 */
	{
		printf(RED"[error]dir_path malloc failed!\n"CLS);
		return -1;
	}
	memset(dir_path, 0, 256);/* 申请的内存块清零 */
	/* 请输入目录名称 */
	/* 获取当前路径(其实不拼接似乎也可以在当前路径正常创建目录) */
	if( (getcwd(dir_path, 256)) == NULL )
	{
		printf(RED"[error]get local pwd failed!\n"CLS);
		return -1;
	}
	else
	{
		printf(GREEN"[ OK  ]get local pwd successfully!\n"CLS);
		strcat(dir_path, "/");
		strcat(dir_path, msg->data);
	}
	/* 创建目录 */
	if( mkdir(dir_path, 0644) == 0 )/* 创建目录成功返回0，目录已存在则会创建失败 */
	{
		printf(GREEN"[ OK  ]create successfully!\n"CLS);
		printf("[new dir path]:%s\n", dir_path);
		msg->result = 1;/* 表示创建成功 */
		/* 清空原来的data成员数据 */
		bzero(msg->data, sizeof(msg->data)/sizeof(char));/* 先清空数据字符串空间 */
		/* 怎么不用strncpy了？因为有警告呀，哈哈 */
		/* https://stackoverflow.com/questions/56782248/gcc-specified-bound-depends-on-the-length-of-the-source-argument */
		strcpy(msg->data, dir_path);
	}
	else
	{
		printf(RED"[error]Failed to create directory(The directory may already exist)!\n"CLS);
		msg->result = -1;/* 表示创建失败 */
	}
	free(dir_path);/* 释放内存 */
	/* 发送回应信息 */
	if( send(accept_fd, msg, sizeof(MSG), 0) < 0)
	{
		perror(RED"[error]send"CLS);
		return -1;
	}
	return 0;
}

/**
 * @Function: deleteServerFileHandle
 * @Description: 在服务器端删除目录或者文件
 * @param accept_fd: 客户端的socket套接字
 * @param msg      : 服务器与客户端通信的数据结构体指针变量
 * @return  : 返回一个整数
 *            0,删除成功;
 *            -1,删除失败
 */
int deleteServerFileHandle(int accept_fd, MSG * msg)
{
	char * file_path;
	msg->result = 0;
	/* 申请内存 */
	if( (file_path = (char *)malloc(256)) == NULL )/* 申请256字节内存空间 */
	{
		printf(RED"[error]file_path malloc failed!\n"CLS);
		return -1;
	}
	memset(file_path, 0, 256);/* 申请的内存块清零 */
	/* 获取当前路径(其实不拼接似乎也可以在当前路径正常创建目录) */
	if( (getcwd(file_path, 256)) == NULL )
	{
		printf(RED"[error]get local pwd failed!\n"CLS);
		return -1;
	}
	else
	{
		printf(GREEN"[ OK  ]get local pwd successfully!\n"CLS);
		strcat(file_path, "/");
		strcat(file_path, msg->data);/* 拼接一个完整的文件路径 */
	}
	printf("[%s]\n", file_path);
	/* 删除文件(删除文件时remove调用unlink,删除目录时，调用rmdir) */
	if( remove(file_path) == 0 )
	{
		printf(GREEN"[ OK  ]delete successfully!\n"CLS);
		msg->result = 1;/* 表示删除成功 */
	}
	else
	{
		printf(RED"[error]Failed to delete file(The file may not exist)!\n"CLS);
		msg->result = -1;/* 表示删除失败 */
	}
	free(file_path);/* 释放内存 */
	/* 发送回应信息 */
	if( send(accept_fd, msg, sizeof(MSG), 0) < 0)
	{
		perror(RED"[error]send"CLS);
		return -1;
	}
	return 0;
}

/**
 * @Function: cdServerDirHandle
 * @Description: 在服务器端切换目录
 * @param accept_fd: 客户端的socket套接字
 * @param msg      : 服务器与客户端通信的数据结构体指针变量
 * @return  : 返回一个整数
 *            0,切换成功;
 *            -1,切换失败
 */
int cdServerDirHandle(int accept_fd, MSG * msg)
{
	char * dir_path;
	/* 申请内存 */
	if( (dir_path = (char *)malloc(256)) == NULL )/* 申请256字节内存空间 */
	{
		printf(RED"[error]dir_path malloc failed!\n"CLS);
		return -1;
	}
	memset(dir_path, 0, 256);/* 申请的内存块清零 */
	/* 获取 cd 后边的路径 */
	char * dirName;
	if( (dirName = strchr(msg->data, ' ')) == NULL)/* 在参数 msg->data 所指向的字符串中搜索第一次出现字符空格（一个无符号字符）的位置。 */
	{
		perror(RED"[error]strchr"CLS);
		return -1;
	}
	else
	{
		while(*(++dirName) == ' ');
	}
	if(dirName == NULL || *dirName == '\0')
	{
		printf(RED"[error]command error!\n"CLS);
		return -1;
	}
	/* 获取本地路径，拼接一个绝对路径 */
	if( (getcwd(dir_path, 256)) == NULL )
	{
		printf(RED"[error]get server local pwd failed!\n"CLS);
		return -1;
	}
	else
	{
		printf(GREEN"[ OK  ]get server local pwd successfully!\n"CLS);
		/* 切换到指定目录 */
		if( strncmp(dirName, "..", 2) != 0)
		{
			strcat(dir_path, "/");
			strcat(dir_path, dirName);
		}
		else /* 返回上一级目录 cd .. */
		{
			/* dir_path= /mnt/hgfs/Sharedfiles/2Linux/01Study/08-LV8/04FTP */
			if( (dirName = strrchr(dir_path, '/')) == NULL)/* 在参数 command 所指向的字符串中搜索第一次出现字符空格（一个无符号字符）的位置。 */
			{
				perror(RED"[error]strrchr"CLS);
				return -1;
			}
			/* dir_path= /mnt/hgfs/Sharedfiles/2Linux/01Study/08-LV8\004FTP */
			*dirName = '\0';/* 将最后一个/替换为 \0 */
			if( strlen(dir_path) == 0)
			{
				memset(dir_path, 0, 256);/* 申请的内存块清零 */
				strncpy(dir_path, "/", strlen("/") + 1);
				printf(YELLOW"[warn ]The server root directory has been reached!\n");
			}
		}
	}
	/* 切换目录 */
	if( chdir(dir_path) == 0 )
	{
		printf(GREEN"[ OK  ]Directory change succeeded!\n"CLS);
		msg->result = 1;
	}
	else
	{
		printf(RED"[error]Failed to change directory!\n"CLS);
		msg->result = -1;
	}
	/* 获取切换后的路径 */
	memset(dir_path, 0, 256);/* 申请的内存块清零 */
	if( (getcwd(dir_path, 256)) == NULL )
	{
		printf(RED"[error]get local pwd(after change) failed!\n"CLS);
		return -1;
	}
	else
	{
		printf(GREEN"[ OK  ]get server local pwd(after change) successfully!\n"CLS);
		printf("[server local PWD+]: %s\n", dir_path);
		bzero(msg->data, sizeof(msg->data)/sizeof(char));/* 先清空数据字符串空间 */
		strcpy(msg->data, dir_path);
	}

	free(dir_path);
	/* 发送回应信息 */
	if( send(accept_fd, msg, sizeof(MSG), 0) < 0)
	{
		perror(RED"[error]send"CLS);
		return -1;
	}

	return 0;
}

/**
 * @Function: clientPutFileHandle
 * @Description: 客户端上传文件到服务器请求处理
 * @param accept_fd: 客户端的socket套接字
 * @param msg      : 服务器与客户端通信的数据结构体指针变量
 * @return  : 返回一个整数
 *            0,上传成功;
 *            -1,上传失败
 */
int clientPutFileHandle(int accept_fd, MSG * msg)
{
	/* 1.显示要上传的文件名称(调用该函数前，已经接收了一次数据) */
	printf(PURPLE"[client(%s:%d)]:"CLS"%s(cliend put file name)\n", msg->client_info[0].ipv4_addr, msg->client_info[0].port, msg->name);
	/* 存储客户端上传的文件名称 */
	char filenameTemp[32] = {0};
	strcpy(filenameTemp, msg->name);
	/* 2.判断要上传的文件在服务器中是否已经存在 */
	while(1)
	{
		/* 2.1检测文件存在状态 */
		if( access(msg->name, F_OK) < 0 )/* 检测文件是否存在(文件不存在的话返回-1) */
		{
			perror(RED"[error]access(can ignore)"CLS);
			sprintf(msg->data, GREEN"[ OK  ]The file "CLS PURPLE"[%s]"CLS GREEN" is not existed!\n"CLS, msg->name);
			msg->result = -1; /* msg->result 设为-1表示服务器端文件不存在，提示客户端重新输入需要下载的文件 */
		}
		else /* 服务器端所在目录有该文件，就需要知客户端覆盖文件还是新建文件用于接收 */
		{
			printf(RED"[error]The file "CLS PURPLE"[%s]"CLS RED" is existed!\n"CLS, msg->name);
			sprintf(msg->data, RED"[error]The file "CLS PURPLE"[%s]"CLS RED" is existed!\n"CLS, msg->name);
			msg->result = 2;
		}
		/* 发送回应信息 */
		if( send(accept_fd, msg, sizeof(MSG), 0) < 0)
		{
			perror(RED"[error]send"CLS);
			return -1;
		}
		/* 接收来自客户端的信息 */
		if( recv(accept_fd, msg, sizeof(MSG),0) < 0)
		{
			perror(RED"[error]recv"CLS);
			return -1;
		}
		if(msg->result == 3)/* 说明可以开始接收信息了 */
			break;
	}
	/* 说明：跳出循环后 msg->result = 3 */
	/* 3.打开要写入的文件 */
	FILE * fpW = NULL;
	if( (fpW = fopen(msg->name, "w+")) == NULL )
	{
		perror(RED"[error]fopen"CLS);
		bzero(msg->data, sizeof(msg->data)/sizeof(msg->data));
		printf(RED"[error]Failed to open the file "CLS PURPLE"[%s]"CLS RED" on the server. !\n"CLS, msg->name);
		sprintf(msg->data, RED"[error]Failed to open the file "CLS PURPLE"[%s]"CLS RED" on the server. !\n"CLS, msg->name);
		msg->result = -1;
		/* 发送回应信息 */
		if( send(accept_fd, msg, sizeof(MSG), 0) < 0)
		{
			perror(RED"[error]send"CLS);
			return -1;
		}
		return -1;
	}
	else
	{
		bzero(msg->data, sizeof(msg->data)/sizeof(msg->data));
		printf(GREEN"[ OK  ]The file "CLS PURPLE"[%s]"CLS GREEN" is opened successfully on server. You can upload the file!\n"CLS, msg->name);
		sprintf(msg->data, GREEN"[ OK  ]The file "CLS PURPLE"[%s]"CLS GREEN" is opened successfully on server. You can upload the file!\n"CLS, msg->name);
		msg->result = 4;
		/* 发送回应信息 */
		if( send(accept_fd, msg, sizeof(MSG), 0) < 0)
		{
			perror(RED"[error]send"CLS);
			return -1;
		}
	}
	rewind(fpW);
	/* 4.开始传输文件 */
	printf(GREEN"[ OK  ]Start receiving files!\n"CLS);
	/* 4.1定义一个缓冲区 */
	char * buffer;
	if( (buffer = (char *)malloc(256)) == NULL )/* 申请128字节内存空间 */
	{
		printf(RED"[error]get buffer memory malloc failed!\n"CLS);
		free(buffer);
		return -1;
	}
	memset(buffer, 0, 256);          /* 申请的内存块清零 */

	/* 4.2告诉客户端开始上传 */
	msg->result = 5;
	if( send(accept_fd, msg, sizeof(MSG), 0) < 0)
	{
		perror(RED"[error]send"CLS);
		return -1;
	}
	/* 4.3开始文件传输 */
	int count = -1;
	while(1)
	{
		/* 4.3.1等待客户端传输一次文件的标志 */
		if( recv(accept_fd, msg, sizeof(MSG),0) < 0)
		{
			perror(RED"[error]recv"CLS);
			return -1;
		}
		if(msg->result == 6)
		{
			bzero(buffer, 256);/* 清空名称字符串空间 */
			/* 4.3.2从socket套接字读取文件数据 */
			if((count = read(accept_fd, buffer, 256)) < 0)
			{
				perror(RED"[error]read"CLS);
				return -1;
			}
			/* 4.3.3写入文件 */
			if( (fwrite(buffer, 1, count, fpW)) < 0)
			{
				perror(RED"[error]fwrite"CLS);
				return -1;
			}
			/* 4.3.4告诉客户端启动下一次传输 */
			msg->result = 5;
			if( send(accept_fd, msg, sizeof(MSG), 0) < 0)
			{
				perror(RED"[error]send"CLS);
				return -1;
			}

		}
		else
		{
			printf(GREEN"[ OK  ]The file "PURPLE"[%s(%s)]"CLS GREEN" has been uploaded!\n"CLS, msg->name, filenameTemp);
			break;
		}
	}
	/* 说明：此处结束 msg->result = 7 */
	fclose(fpW);
	free(buffer);
	return 0;
}


/**
 * @Function: clientGetFileHandle
 * @Description: 客户端从服务器下载文件请求处理
 * @param accept_fd: 客户端的socket套接字
 * @param msg      : 服务器与客户端通信的数据结构体指针变量
 * @return  : 返回一个整数
 *            0,下载成功;
 *            -1,下载失败
 */
int clientGetFileHandle(int accept_fd, MSG * msg)
{
	/* 1.显示要下载的文件名称(调用该函数前，已经接收了一次数据) */
	printf(PURPLE"[client(%s:%d)]:"CLS"%s(cliend download file name)\n", msg->client_info[0].ipv4_addr, msg->client_info[0].port, msg->name);
	/* 2.判断要下载的文件是否在服务器端存在 */
	while(1)
	{
		/* 2.1判断文件是否存在 */
		if( access(msg->name, F_OK) < 0 )/* 检测文件是否存在(文件不存在的话返回-1) */
		{
			perror(RED"[error]access"CLS);
			sprintf(msg->data, RED"[error]The file [%s] is not existed!\n"CLS, msg->name);
			msg->result = -1; /* msg->result 设为-1表示服务器端文件不存在，提示客户端重新输入需要下载的文件 */
		}
		else
		{
			printf(GREEN"[ OK  ]The file [%s] is existed!\n"CLS, msg->name);
			sprintf(msg->data, GREEN"[ OK  ]The file [%s] is existed!\n"CLS, msg->name);
			msg->result = 2;
		}
		/* 2.2向客户端发送消息，告知服务器文件状态 */
		if( send(accept_fd, msg, sizeof(MSG), 0) < 0)
		{
			perror(RED"[error]send"CLS);
			return -1;
		}
		/* 2.3接收来自客户端的信息 */
		if( recv(accept_fd, msg, sizeof(MSG),0) < 0)
		{
			perror(RED"[error]recv"CLS);
			return -1;
		}
		if(msg->result == 3)
			break;
	}
	/* 说明：跳出循环后 msg->result = 3 */
	/* 3.打开要下载的文件 */
	int fdR = -1;
	if( (fdR = open(msg->name, O_RDONLY)) < 0 )
	{
		perror(RED"[error]open"CLS);
		msg->result = -1;
		printf(RED"[error]File [%s] opening failure!\n"CLS, msg->name);
		sprintf(msg->data, GREEN"[ OK  ]File [%s] opened successfully!\n"CLS, msg->name);
		if( send(accept_fd, msg, sizeof(MSG), 0) < 0)
		{
			perror(RED"[error]send"CLS);
			return -1;
		}
		return -1;
	}
	else
	{
		msg->result = 4;/* 表示服务器端文件打开成功 */
		printf(GREEN"[ OK  ]File [%s] opened successfully!\n"CLS, msg->name);
		sprintf(msg->data, GREEN"[ OK  ]File [%s] opened successfully!\n"CLS, msg->name);
		if( send(accept_fd, msg, sizeof(MSG), 0) < 0)
		{
			perror(RED"[error]send"CLS);
			return -1;
		}
	}
	/* 说明：此处结束 msg->result = 4 */
	/* 4.等待等待客户端打开接收文件 */
	bzero(msg->data, sizeof(msg->data)/sizeof(char));/* 先清空数据字符串空间 */
	if( recv(accept_fd, msg, sizeof(MSG),0) < 0)
	{
		perror(RED"[error]recv"CLS);
		return -1;
	}
	printf(PURPLE"[client(%s:%d)]:"CLS"%s\n", msg->client_info[0].ipv4_addr, msg->client_info[0].port, msg->data);
	if(msg->result < 0)
		return -1;
	/* 说明：此处结束 msg->result = 5 */
	/* 5.开始传输文件 */
	int count = -1;
	/* 说明：为什么不用mag->data传输数据？因为老乱码，即便每次传输客户端和服务器都有标志，但是依然有乱码，目前未知原因，后边再改进把 */
	char * buffer;
	if( (buffer = (char *)malloc(256)) == NULL )/* 申请128字节内存空间 */
	{
		printf(RED"[error]get buffer memory malloc failed!\n"CLS);
		free(buffer);
		return -1;
	}
	memset(buffer, 0, 256);          /* 申请的内存块清零 */

	while(1)
	{
		/* 5.1等待客户端可以开始文件开始传输的标志 */
		if( recv(accept_fd, msg, sizeof(MSG),0) < 0)
		{
			perror(RED"[error]recv"CLS);
			return -1;
		}
		/* 5.2处理客户端标志信息 */
		if(msg->result == 6)
		{
			bzero(buffer, 256);/* 清空名称字符串空间 */
			/* 5.2.1从文件描述符读取服务器文件数据 */
			if( (count = read(fdR, buffer, 256)) <= 0)
			{
				if( count == 0)
				{
					printf(GREEN"[ OK  ]File [%s] reading finished!\n"CLS, msg->name);
					msg->result = 8;
					/* 发送文件读取结束标志 */
					if( send(accept_fd, msg, sizeof(MSG), 0) < 0)
					{
						perror(RED"[error]send"CLS);
						return -1;
					}
				}
				else
					perror(RED"[error]read"CLS);
				break;
			}
			/* 5.2.2发送一个可以从套接字读取数据的标志 */
			msg->result = 7;
			if( send(accept_fd, msg, sizeof(MSG), 0) < 0)
			{
				perror(RED"[error]send"CLS);
				return -1;
			}
			/* 5.2.3写入数据到socket套接字 */
			if(write(accept_fd, buffer, count) < 0)
			{
				perror(RED"[error]write"CLS);
				break;
			}
		}
	}

	/* 说明：此处结束 msg->result = 8 */
	close(fdR);/* 关闭读取文件的文件描述符 */
	free(buffer);
	return 0;
}