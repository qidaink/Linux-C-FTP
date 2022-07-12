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
#include <stdio.h>      /* perror scanf printf */
#include <stdlib.h>     /* exit   atoi  system */
#include <unistd.h>     /* sleep  read  close getpid fork stat getcwd chdir*/
#include <errno.h>      /* errno号 */

#include <sys/types.h>  /* socket getpid    bind listen accept send fork stat mkdir */
#include <sys/socket.h> /* socket inet_addr bind listen accept send */
#include <arpa/inet.h>  /* htons  inet_addr inet_pton */
#include <netinet/in.h> /* ntohs  inet_addr inet_ntop*/

#include <string.h>     /* bzero strncasecmp strlen memset */

#include <signal.h>     /* signal */
#include <sys/wait.h>   /* waitpid */

#include <dirent.h>    /* opendir readdir*/
#include <sys/stat.h>  /* stat mkdir */
#include <time.h>      /* localtime localtime_r */
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
	int result;/* 标记是否成功,-1,失败;0,成功 */
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
		printf(PURPLE"[client(%s:%d)]:"CLS" msg type:%d\n", clientInfo.ipv4_addr, clientInfo.port, msg.type);
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


	msg->result = 0;/* 表示获取信息成功 */
	/* 发送回应信息 */
	if( send(accept_fd, msg, sizeof(MSG), 0) < 0)
	{
		perror(RED"[error]send"CLS);
		return -1;
	}
	return msg->result;
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
	bzero(msg->data, sizeof(msg->data));/* 先清空IP地址存储空间信息 */
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
	/* 打印所有文件名 */
	while( (dirInfo = readdir(dir)) != NULL )
	{
		/* 清空原来的data成员数据 */
		bzero(msg->data, sizeof(msg->data));/* 先清空数据字符串空间 */
		/* 跳过 . 和 ..  */
		if(strcmp(dirInfo->d_name, ".") == 0 || strcmp(dirInfo->d_name, "..") == 0)
		{
			continue;
		}
		else
		{
			getFileInfo(dirInfo->d_name, msg->data);/* 此处获取并打印文件详细信息 */
			msg->result = -1;
			/* 发送回应信息 */
			if( send(accept_fd, msg, sizeof(MSG), 0) < 0)
			{
				perror(RED"[error]send"CLS);
				return -1;
			}
		}
	}
	/* 清空原来的data成员数据 */
	bzero(msg->data, sizeof(msg->data));/* 先清空数据字符串空间 */
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
	msg->result = 0;/* 以此表示文件读取全部结束 */
	if( send(accept_fd, msg, sizeof(MSG), 0) < 0)
	{
		perror(RED"[error]send"CLS);
		return -1;
	}

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
		msg->result = 0;/* 表示创建成功 */
		/* 清空原来的data成员数据 */
		bzero(msg->data, sizeof(msg->data));/* 先清空数据字符串空间 */
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
		msg->result = 0;/* 表示删除成功 */
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
		msg->result = 0;
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
		bzero(msg->data, sizeof(msg->data));/* 先清空数据字符串空间 */
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
