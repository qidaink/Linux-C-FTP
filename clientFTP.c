/** =====================================================
 * Copyright © hk. 2022-2025. All rights reserved.
 * File name: clientFTP.c
 * Author   : qidaink
 * Date     : 2022-07-10
 * Version  :
 * Description:
 * Others   :
 * Log      :
 * ======================================================
 */ 

/* 头文件 */
#include <stdio.h>     /* perror scanf printf gets */
#include <stdlib.h>    /* exit   atoi  system malloc free*/
#include <unistd.h>    /* sleep getcwd stat chdir */

#include <sys/types.h> /* socket opendir   connect send stat mkdir */
#include <sys/socket.h>/* socket inet_addr connect send */
#include <netinet/in.h>/* inet_addr */
#include <arpa/inet.h> /* inet_addr inet_pton htonl*/

#include <string.h>    /* bzero strncasecmp strlen memset */
#include <dirent.h>    /* opendir readdir*/

#include <sys/stat.h>  /* stat mkdir */
#include <time.h>      /* localtime localtime_r */
#include <string.h>    /* strchr */
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
	int type;
	char name[N];
	char data[256];
	CLIENT_INFO client_info[2];/* 存放客户端和服务器端信息 */
	int result;/* 标记是否成功 */
} MSG;


int clientMainMenu(void);     /* 客户端主菜单 */
/* 客户端连接服务器 */
int clientConnectServer(MSG * msg);/* 客户端连接服务器 */
int clientConnectInfo(void);  /* 客户端连接服务器提示信息界面 */
int establishConnect(char *serverIP, int serverPort, char *clientIP, int clientPort, int mode);/* 建立TCP连接 */
int getClientConnectInfo(int socket_fd, MSG * msg);/* 从服务器获取客户端IP和端口请求 */
/* 客户端本地操作 */
int clientLocalMenu(void);       /* 客户端本地操作主菜单 */
int clientLocalFunc(void);       /* 客户端本地操作功能客户端本地操作功能 */
int getClientLocalPWD(void);     /* 获取客户端当前路径 */
int getFileInfo(const char * filename);/* 获取指定文件信息 */
int getClientLocalFileList(void);/* 获取客户端当前路径下所有文件名 */
int createClientDir(void);       /* 在客户端当前路径创建目录 */
int deleteClientFile(void);      /* 在客户端当前路径删除一个目录或者文件 */
int cdClientDir(void);           /* 在客户端切换到当前路径下的一个子目录 */
/* 客户端在服务器的操作 */
int serverFuncMenu(void);                        /* 客户端操作服务器功能菜单 */
int serverFunc(int socket_fd, MSG * msg);        /* 客户端操作服务器功能实现 */
int getServerLocalPWD(int socket_fd, MSG * msg); /* 获取服务器当前路径 */
int getServerFileList(int socket_fd, MSG * msg); /* 获取服务器当前路径下所有文件 */
int createServerDir(int socket_fd, MSG * msg);   /* 在服务器当前路径创建目录 */
int deleteServerFile(int socket_fd, MSG * msg);  /* 在服务器当前路径删除一个文件或者目录 */
int cdServerDir(int socket_fd, MSG * msg);       /* 切换到服务器指定路径 */


int main(int argc, char *argv[])
{
	char ch;
	int num = 0; /* 接收用户选择序号的变量 */
	int socket_fd = -1;/* socket 套接字 */
	MSG msg;           /* 与服务器通信的结构体(每次修改的都是type和data成员，其他的基本都是不变) */
	/* 1.连接服务器 */
	socket_fd = clientConnectServer(&msg);
	while(1)
	{
		clientMainMenu(); /* 打印主菜单 */
		printf("please choose:");
		if(scanf("%d", &num) < 0 ) /* 输入的是一个整数，最好还是清理一下缓冲区，不然很有可能“污染”后边的数据 */
		{
			perror(RED"[error]scanf"CLS);
			exit(-1);
		}
		while ((ch = getchar()) != EOF && ch != '\n') ; /* 清除缓冲区的多余字符内容 */
		switch(num)
		{
		case 1:/* 客户端操作菜单 */
			clientLocalFunc();
			break;
		case 2:/* 服务器操作菜单 */
			serverFunc(socket_fd, &msg);
			break;
		case 3:/* 退出进程 */
			send(socket_fd, "quit", sizeof("quit"),0);
			close(socket_fd);
			exit(0);
			break;
		default:
			printf(RED"Invalid data cmd!\n"CLS);
			break;
		}
	}

	return 0;
}
/*=================================================*/
/**
 * @Function: clientMainMenu
 * @Description: 客户端主菜单
 * @param   : none
 * @return  : 返回一个整数
 *            0,显示成功;
 *            -1,显示失败
 */
int clientMainMenu(void)
{
	printf(BOLD  "------------------- client main menu ----------------------\n"CLS);
	printf(YELLOW"| 1.client              2.server\n"CLS);
	printf(YELLOW"| 3.quit\n"CLS);
	printf(BOLD  "-----------------------------------------------------------\n"CLS);
	return 0;
}
/*=================================================*/
/**
 * @Function: clientConnectInfo
 * @Description: 客户端连接服务器提示信息
 * @param   : none
 * @return  : 返回一个整数
 *            0,显示成功;
 *            -1,显示失败
 */
int clientConnectInfo(void)
{
	printf(BOLD  "---------------- Connection Description -------------------\n"CLS);
	printf(YELLOW"|(1)First, We should enter the IP and port of the server.\n"CLS);
	printf(YELLOW"|(2)Second, We should enter the IP and port of the client.\n"CLS);
	printf(YELLOW"|\n"CLS);
	printf(YELLOW"|"CLS GREEN"serverIP: IP address bound to the server.\n"CLS);
	printf(YELLOW"|"CLS GREEN"serverPort: >5000.\n"CLS);
	printf(YELLOW"|"CLS GREEN"clientIP: IP address of the local NIC(or 127.0.0.1).\n"CLS);
	printf(YELLOW"|"CLS GREEN"clientPort: >5000 and != serverPort.\n"CLS);
	printf(YELLOW"|"CLS GREEN"mode: 0,The client is not bound to a fixed IP address or port;\n"CLS);
	printf(YELLOW"|"CLS GREEN"      1,opposite\n"CLS);
	printf(BOLD  "-----------------------------------------------------------\n"CLS);
	return 0;
}

/**
 * @Function: clientConnectServer
 * @Description: 客户端连接服务器
 * @param   : none
 * @return  : 返回一个整数
 *            socket_fd,连接成功,并返回创建的socket套接字;
 *            -1,连接失败
 */
int clientConnectServer(MSG * msg)
{
	char ch;
	char confirm;
	int socket_fd = -1;
	char serverIP[16] = {0}; /* 服务器端IP地址 */
	char clientIP[16] = {0}; /* 客户端IP地址 */
	int serverPort = -1; /* 服务器端口 */
	int clientPort = -1; /* 客户端端口 */
	int mode = -1;/* 是否绑定固定的IP和端口 */
	/* 1.打印提示信息 */
	clientConnectInfo();
	printf("Whether to use the default settings(Y or N):");
	if( scanf("%c", &confirm) < 0)/* 获取输入数据 */
	{
		perror(RED"[error]scanf"CLS);
		return -1;
	}
	while ((ch = getchar()) != EOF && ch != '\n') ; /* 清除缓冲区的多余字符内容 */
	if(confirm == 'Y' || confirm == 'y')
	{
		strncpy(serverIP, "192.168.10.101", 16);
		serverPort = 5001;
		strncpy(clientIP, "127.0.0.1", 16);
		clientPort = 5002;
		mode = 0;/* 默认不绑定固定IP和端口 */
	}
	else
	{
		/* 2.输入服务器端IP地址和端口——用于连接服务器 */
		printf("serverIP serverPort: ");
		if( scanf("%s %d", serverIP, &serverPort) < 0)
		{
			perror(RED"[error]scanf"CLS);
			return -1;
		}
		while ((ch = getchar()) != EOF && ch != '\n') ; /* 清除缓冲区的多余字符内容 */
		/* 3.输入客户端要绑定的IP和端口——用于设置自身的IP和端口号 */
		printf("clientIP clientPort mode: ");
		if( scanf("%s %d %d", clientIP, &clientPort, &mode) < 0)
		{
			perror(RED"[error]scanf"CLS);
			return -1;
		}
		while ((ch = getchar()) != EOF && ch != '\n') ; /* 清除缓冲区的多余字符内容 */
	}
	printf(GREEN"[The entered data]:\n"CLS);
	printf("server:"GREEN"%s %d\n"CLS, serverIP, serverPort);
	if(mode == 0)
	{
		printf("client:"RED"%s %d(Server return is required!)\n"CLS, clientIP, clientPort);
		printf(GREEN"[ OK  ]client staring ... OK!\n"CLS);
	}
	else
	{
		printf("client:"GREEN"%s %d\n"CLS, clientIP, clientPort);
		printf(GREEN"[ OK  ]client[%s:%d]"CLS"staring...OK!\n"CLS, clientIP, clientPort);
	}
	/* 提示客户端启动完成，下一步将是连接服务器端 */
	/* 3.建立TCP连接 */
	printf("connecting to the server ...\n");
	socket_fd = establishConnect(serverIP, serverPort, clientIP, clientPort, mode);
	/* 4.服务器回传客户端IP和端口信息 */
	if(socket_fd > 0)/* 连接成功 */
	{
		printf(GREEN"[ OK  ]connecting to the server successfully!\n"CLS);
		getClientConnectInfo(socket_fd, msg);
	}
	else
		printf(RED"[error]Failed to connect to server!\n"CLS);
	return socket_fd;
}

/**
 * @Function: establishConnect
 * @Description: 与服务器建立TCP连接
 * @param serverIP   : 服务器IP
 * @param serverPort : 服务器端口
 * @param clientIP   : 客户端IP
 * @param clientPort : 客户端端口
 * @param mode       : 模式；0,表示客户端不绑定固定IP和端口(端口随机分配);1,表示客户端绑定固定的IP和端口
 * @return  : 返回一个整数
 *            socket_fd,连接成功,并返回创建的socket套接字;
 *            -1,连接失败
 */
int establishConnect(char *serverIP, int serverPort, char *clientIP, int clientPort, int mode)
{
	/* 1.创建套接字，得到套接字描述符 */
	int socket_fd = -1; /* 接收服务器端socket描述符 */
	if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror(RED"[error]socket"CLS);
		return -1;
	}
	/* 2.网络属性设置 */
	/* 2.1允许绑定地址快速重用 */
	int b_reuse = 1;
	setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &b_reuse, sizeof (int));
	if(mode == 1)
	{
		/* 3.客户端绑定固定的 IP 和 端口号 (是可选的)*/
		/* 3.1填充结构体 */
		struct sockaddr_in sinClient;
		bzero(&sinClient, sizeof (sinClient)); /* 将内存块（字符串）的前n个字节清零 */
		sinClient.sin_family = AF_INET;   /* 协议族 */
		sinClient.sin_port = htons(clientPort); /* 网络字节序的端口号 */
		if(inet_pton(AF_INET, clientIP, (void *)&sinClient.sin_addr) != 1)/* IP地址 */
		{
			perror(RED"[error]inet_pton"CLS);
			return -1;
		}
		/* 3.2绑定 */
		if(bind(socket_fd, (struct sockaddr *)&sinClient, sizeof(sinClient)) < 0)
		{
			perror(RED"[error]bind"CLS);
			return -1;
		}
	}
	/* 4.连接服务器 */
	/* 4.1填充struct sockaddr_in结构体变量 */
	struct sockaddr_in sin;
	bzero (&sin, sizeof(sin)); /* 将内存块（字符串）的前n个字节清零 */
	sin.sin_family = AF_INET;   /* 协议族 */
	sin.sin_port = htons(serverPort); /* 网络字节序的端口号 */
	/* 客户端的需要与系统网卡的IP一致 */
	if(inet_pton(AF_INET, serverIP, (void *)&sin.sin_addr) != 1)/* IP地址 */
	{
		perror(RED"[error]inet_pton"CLS);
		return -1;
	}
	/* 4.2连接 */
	if(connect(socket_fd, (struct sockaddr *)&sin, sizeof(sin)) < 0)
	{
		perror(RED"[error]connect"CLS);
		return -1;
	}
	return socket_fd;
}

/**
 * @Function: getClientConnectInfo
 * @Description: 从服务器获取客户端IP和端口请求
 * @param socket_fd: 客户端的socket套接字
 * @param msg      : 服务器与客户端通信的数据结构体指针变量
 * @return  : 返回一个整数
 *            0,获取成功;
 *            -1,获取失败
 */
int getClientConnectInfo(int socket_fd, MSG * msg)
{
	msg->type = CONNECT_INFO;/* 设置通信的消息类型为0，表示请求获取客户端IP和端口号	*/
	
	/* 发送获取客户端IP和端口号的数据 */
	if( send(socket_fd, msg, sizeof(MSG),0) < 0)
	{
		perror(RED"[error]send"CLS);
		return -1;
	}
	/* 等待服务器回传数据 */
	if( recv(socket_fd, msg, sizeof(MSG),0) < 0)
	{
		perror(RED"[error]recv"CLS);
		return -1;
	}
	printf(PURPLE"[server reply]:\n"CLS);
	printf("client information:%s %d\n", msg->client_info[0].ipv4_addr,  msg->client_info[0].port);
	printf("server information:%s %d\n", msg->client_info[1].ipv4_addr,  msg->client_info[1].port);

	return 0;
}

/**
 * @Function: clientLocalMenu
 * @Description: 客户端本地操作主菜单
 * @param   : none
 * @return  : 返回一个整数
 *            0,显示成功;
 *            -1,显示失败
 */
int clientLocalMenu(void)
{
	printf(BOLD  "------------------- client local menu ----------------------\n"CLS);
	printf(YELLOW"| 1.pwd              2.ls\n"CLS);
	printf(YELLOW"| 3.mkdir            4.rm\n"CLS);
	printf(YELLOW"| 5.cd               6.quit\n"CLS);
	printf(BOLD  "-----------------------------------------------------------\n"CLS);
	return 0;
}

/**
 * @Function: clientLocalFunc
 * @Description: 客户端本地操作功能
 * @param   : none
 * @return  : 返回一个整数
 *            0,成功;
 *            -1,失败
 */
int clientLocalFunc(void)
{
	char ch;
	int num = 0; /* 接收用户选择序号的变量 */
	while(1)
	{
		clientLocalMenu(); /* 打印主菜单 */
		printf("please choose:");
		if(scanf("%d", &num) < 0 ) /* 输入的是一个整数，最好还是清理一下缓冲区，不然很有可能“污染”后边的数据 */
		{
			perror(RED"[error]scanf"CLS);
			exit(-1);
		}
		while ((ch = getchar()) != EOF && ch != '\n') ; /* 清除缓冲区的多余字符内容 */
		switch(num)
		{
		case 1:/* 显示客户端当前路径 pwd */
			getClientLocalPWD();
			break;
		case 2:/* 显示客户端当前路径下文件 ls */
			getClientLocalFileList();
			break;
		case 3:/* 创建目录 mkdir*/
			createClientDir();
			break;
		case 4:/* 删除文件 rm */
			deleteClientFile();
			break;
		case 5:/* 切换目录 cd */
			cdClientDir();
			break;
		case 6:/* 退出本地功能菜单*/
			return 0;
			break;
		default:
			printf(RED"Invalid data cmd!\n"CLS);
			break;
		}
	}

	return 0;
}

/**
 * @Function: getClientLocalPWD
 * @Description: 获取客户端当前路径
 * @param   : none
 * @return  : 返回一个整数
 *            0,获取成功;
 *            -1,获取失败
 */
int getClientLocalPWD(void)
{
	// msg->type = LOCAL_PWD;/* 设置通信的消息类型为1 */
	char * client_current_dir;/* 存储客户端当前路径 */
	/* 申请内存空间 */
	if( (client_current_dir = (char *)malloc(512)) == NULL)/* 申请512字节内存空间 */
	{
		printf(RED"[error]client_current_dir malloc failed!\n"CLS);
		return -1;
	}
	memset(client_current_dir, 0, 512);/* 申请的内存块清零 */
	/* 获取当前路径 */
	if( (getcwd(client_current_dir, 512)) == NULL )
	{
		printf(RED"[error]get local pwd failed!\n"CLS);
		return -1;
	}
	else
	{
		printf(GREEN"[ OK  ]get local pwd successfully!\n"CLS);
		printf("[client local PWD]: %s\n", client_current_dir);
	}
	free(client_current_dir);/* 释放内存空间 */
	return 0;
}
/**
 * @Function: getFileInfo
 * @Description: 获取指定文件信息
 * @param filename: 文件名称(可以包含路径)
 * @return  : 返回一个整数
 *            0,获取成功;
 *            -1,获取失败
 */
// -rwxrwxrwx 2092 2022-5-8 19:47 filename
int getFileInfo(const char * filename)
{
	/* 定义一个文件属性结构体变量 */
	struct stat file_attr;
	int i = 0;
	struct tm *t;
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
	}
	if( S_ISDIR(file_attr.st_mode) )
	{
		printf("d");
	}
	if( S_ISCHR(file_attr.st_mode) )
	{
		printf("c");
	}
	if( S_ISBLK(file_attr.st_mode) )
	{
		printf("b");
	}
	if( S_ISFIFO(file_attr.st_mode) )
	{
		printf("p");
	}
	if( S_ISSOCK(file_attr.st_mode) )
	{
		printf("s");
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
					break;
				case 1:
					printf("w");
					break;
				case 2:
					printf("r");
					break;
			}
		}
		else
		{
			printf("-");
		}
	}
	/* 5. 文件字节数(文件大小) */
	printf(" %5.1fK",(float)file_attr.st_size / 1024);
	/* 6. 打印文件修改时间 */
	t = localtime(&file_attr.st_ctime);
	printf(" %d-%02d-%02d %02d:%02d",t->tm_year+1900,t->tm_mon+1,t->tm_mday,t->tm_hour,t->tm_min);
	if( S_ISDIR(file_attr.st_mode) )
		printf(GREEN" %s\n"CLS, filename);
	else
		printf(" %s\n", filename);
	return 0;
}


/**
 * @Function: getClientLocalFileList
 * @Description: 获取客户端当前路径下所有文件名
 * @param   : none
 * @return  : 返回一个整数
 *            0,获取成功;
 *            -1,获取失败
 */
int getClientLocalFileList(void)
{
	int i = 0;
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
		/* 跳过 . 和 ..  */
		if(strcmp(dirInfo->d_name, ".") == 0 || strcmp(dirInfo->d_name, "..") == 0)
		{
			continue;
		}
		else
		{
			i++;
			getFileInfo(dirInfo->d_name);/* 此处获取并打印文件详细信息 */
		}
	}
	puts("");
	closedir(dir);
	return 0;
}

/**
 * @Function: createClientDir
 * @Description: 在客户端创建目录
 * @param   : none
 * @return  : 返回一个整数
 *            0,创建成功;
 *            -1,创建失败
 */
int createClientDir(void)
{
	char ch;
	char dir_name[32];
	char * dir_path;
	/* 申请内存 */
	if( (dir_path = (char *)malloc(256)) == NULL )/* 申请256字节内存空间 */
	{
		printf(RED"[error]dir_path malloc failed!\n"CLS);
		return -1;
	}
	memset(dir_path, 0, 256);/* 申请的内存块清零 */
	/* 请输入目录名称 */
	printf("Please enter file name:");
	if( scanf("%s", dir_name) < 0)/* 获取输入数据 */
	{
		perror(RED"[error]scanf"CLS);
		return -1;
	}
	while ((ch = getchar()) != EOF && ch != '\n') ; /* 清除缓冲区的多余字符内容 */
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
		strcat(dir_path, dir_name);
	}
	/* 创建目录 */
	if( mkdir(dir_name, 0644) == 0 )/* 创建目录成功返回0，目录已存在则会创建失败 */
	{
		printf(GREEN"[ OK  ]create successfully!\n"CLS);
		printf("[new dir path]:%s\n", dir_path);
	}
	else
		printf(RED"[error]Failed to create directory(The directory may already exist)!\n"CLS);
	free(dir_path);
	return 0;
}

/**
 * @Function: deleteClientFile
 * @Description: 在客户端删除目录或者文件
 * @param   : none
 * @return  : 返回一个整数
 *            0,删除成功;
 *            -1,删除失败
 */
int deleteClientFile(void)
{
	char ch;
	char confirm;
	char file_name[32];
	char * file_path;
	/* 申请内存 */
	if( (file_path = (char *)malloc(256)) == NULL )/* 申请256字节内存空间 */
	{
		printf(RED"[error]file_path malloc failed!\n"CLS);
		return -1;
	}
	memset(file_path, 0, 256);/* 申请的内存块清零 */
	/* 请输入文件名称 */
	printf("Please enter file name:");
	if( scanf("%s", file_name) < 0)/* 获取输入数据 */
	{
		perror(RED"[error]scanf"CLS);
		return -1;
	}
	while ((ch = getchar()) != EOF && ch != '\n') ; /* 清除缓冲区的多余字符内容 */
	/* 获取当前路径(其实不拼接似乎也可以在当前路径正常删除目录) */
	if( (getcwd(file_path, 256)) == NULL )
	{
		printf(RED"[error]get local pwd failed!\n"CLS);
		return -1;
	}
	else
	{
		printf(GREEN"[ OK  ]get local pwd successfully!\n"CLS);
		strcat(file_path, "/");
		strcat(file_path, file_name);
	}
	/* 选择是否确认删除 */
	printf(YELLOW"[warn ]The file path to be deleted is:\n[%s]\n"CLS, file_path);
	printf("Are you sure to delete this file(Y or N):");
	if( scanf("%c", &confirm) < 0)/* 获取输入数据 */
	{
		perror(RED"[error]scanf"CLS);
		return -1;
	}
	while ((ch = getchar()) != EOF && ch != '\n') ; /* 清除缓冲区的多余字符内容 */
	if(confirm == 'N' || confirm == 'n')
	{
		printf(YELLOW"[warn ]Deletion of this file has been cancelled!\n"CLS);
		return -1;
	}
	/* 删除文件(删除文件时remove调用unlink,删除目录时，调用rmdir) */
	if( remove(file_name) == 0 )
	{
		printf(GREEN"[ OK  ]delete successfully!\n"CLS);
	}
	else
		printf(RED"[error]Failed to delete file(The file may not exist)!\n"CLS);
	free(file_path);
	return 0;
}

/**
 * @Function: cdClientDir
 * @Description: 在客户端下切换到当前路径下的目录中
 * @param   : none
 * @return  : 返回一个整数
 *            0,删除成功;
 *            -1,删除失败
 */
int cdClientDir(void)
{
	char ch;
	char command[128];
	char * dir_path;
	/* 申请内存 */
	if( (dir_path = (char *)malloc(256)) == NULL )/* 申请256字节内存空间 */
	{
		printf(RED"[error]dir_path malloc failed!\n"CLS);
		return -1;
	}
	memset(dir_path, 0, 256);/* 申请的内存块清零 */
	/* 请输入目录名称 */
	printf("Please enter cd command(cd path):");
	if( scanf("%[^\n]", command) < 0 )/* 获取输入数据，可以带空格 */
	{
		perror(RED"[error]gets"CLS);
		return -1;
	}
	while ((ch = getchar()) != EOF && ch != '\n') ; /* 清除缓冲区的多余字符内容 */
	printf("command = %s\n\n", command);
	/* 获取 cd 后边的路径 */
	char * dirName;
	if( (dirName = strchr(command, ' ')) == NULL)/* 在参数 command 所指向的字符串中搜索第一次出现字符空格（一个无符号字符）的位置。 */
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
	/* 获取当前路径 */
	if( (getcwd(dir_path, 256)) == NULL )
	{
		printf(RED"[error]get client local pwd failed!\n"CLS);
		return -1;
	}
	else
	{
		printf(GREEN"[ OK  ]get client local pwd successfully!\n"CLS);
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
				strncpy(dir_path, "/", 2);
				printf(YELLOW"[warn ]The root directory has been reached!\n");
			}	
		}
	}
	/* 切换目录 */
	if( chdir(dir_path) == 0 )
	{
		printf(GREEN"[ OK  ]Directory change succeeded!\n"CLS);
	}
	else
		printf(RED"[error]Failed to change directory!\n"CLS);
	/* 打印当前路径 */
	memset(dir_path, 0, 256);/* 申请的内存块清零 */
	if( (getcwd(dir_path, 256)) == NULL )
	{
		printf(RED"[error]get client local pwd(after change) failed!\n"CLS);
		return -1;
	}
	else
	{
		printf(GREEN"[ OK  ]get client local pwd(after change) successfully!\n"CLS);
		printf("[client local PWD]: %s\n", dir_path);
	}
	free(dir_path);
	return 0;
}

/**
 * @Function: serverFuncMenu
 * @Description: 服务器操作菜单
 * @param   : none
 * @return  : 返回一个整数
 *            0,显示成功;
 *            -1,显示失败
 */
int serverFuncMenu(void)
{
	printf(BOLD  "------------------- client server menu ---------------------\n"CLS);
	printf(YELLOW"| 1.pwd              2.ls\n"CLS);
	printf(YELLOW"| 3.mkdir            4.rm\n"CLS);
	printf(YELLOW"| 5.cd               6.quit\n"CLS);
	printf(BOLD  "-----------------------------------------------------------\n"CLS);
	return 0;
}

/**
 * @Function: serverFunc
 * @Description: 客户端对服务器相关功能
 * @param socket_fd: 客户端的socket套接字
 * @param msg      : 服务器与客户端通信的数据结构体指针变量
 * @return  : 返回一个整数
 *            0,成功;
 *            -1,失败
 */
int serverFunc(int socket_fd, MSG * msg)
{
	char ch;
	int num = 0; /* 接收用户选择序号的变量 */
	while(1)
	{
		serverFuncMenu(); /* 打印主菜单 */
		printf("please choose:");
		if(scanf("%d", &num) < 0 ) /* 输入的是一个整数，最好还是清理一下缓冲区，不然很有可能“污染”后边的数据 */
		{
			perror(RED"[error]scanf"CLS);
			exit(-1);
		}
		while ((ch = getchar()) != EOF && ch != '\n') ; /* 清除缓冲区的多余字符内容 */
		switch(num)
		{
		case 1:/* 显示服务器当前路径 pwd */
			getServerLocalPWD(socket_fd, msg);
			break;
		case 2:/* 显示服务器当前路径下文件 ls */
			getServerFileList(socket_fd, msg);
			break;
		case 3:/* 创建目录 mkdir*/
			createServerDir(socket_fd, msg);
			break;
		case 4:/* 删除文件 rm */
			deleteServerFile(socket_fd, msg);
			break;
		case 5:/* 切换目录 cd */
			cdServerDir(socket_fd, msg);
			break;
		case 6:/* 退出服务器功能菜单*/
			return 0;
			break;
		default:
			printf(RED"Invalid data cmd!\n"CLS);
			break;
		}
	}

	return 0;
}

/**
 * @Function: getServerLocalPWD
 * @Description: 获取服务器当前路径
 * @param socket_fd: 客户端的socket套接字
 * @param msg      : 服务器与客户端通信的数据结构体指针变量
 * @return  : 返回一个整数
 *            0,获取成功;
 *            -1,获取失败
 */
int getServerLocalPWD(int socket_fd, MSG * msg)
{
	msg->type = SERVER_PWD;/* 设置通信的消息类型为SERVER_PWD,表示请求获取服务器当前路径 */
	
	/* 发送获取客户端IP和端口号的数据 */
	if( send(socket_fd, msg, sizeof(MSG),0) < 0)
	{
		perror(RED"[error]send"CLS);
		return -1;
	}
	/* 等待服务器回传数据 */
	if( recv(socket_fd, msg, sizeof(MSG),0) < 0)
	{
		perror(RED"[error]recv"CLS);
		return -1;
	}
	printf(PURPLE"[server reply]:"CLS"\n[server local PWD]:%s\n", msg->data);

	return 0;
}

/**
 * @Function: getServerFileList
 * @Description: 获取服务器当前路径下所有文件
 * @param socket_fd: 客户端的socket套接字
 * @param msg      : 服务器与客户端通信的数据结构体指针变量
 * @return  : 返回一个整数
 *            0,获取成功;
 *            -1,获取失败
 */
int getServerFileList(int socket_fd, MSG * msg)
{
	msg->type = SERVER_LS;/* 设置通信的消息类型为SERVER_LS,表示请求获取服务器当前路径下文件列表 */
	msg->result = -1;
	/* 发送请求类型 */
	if( send(socket_fd, msg, sizeof(MSG),0) < 0)
	{
		perror(RED"[error]send"CLS);
		return -1;
	}
	printf(PURPLE"[server reply]:\n"CLS);
	while(1)
	{
		/* 等待服务器回传数据 */
		if( recv(socket_fd, msg, sizeof(MSG),0) < 0)
		{
			perror(RED"[error]recv"CLS);
			return -1;
		}
		printf("%s\n", msg->data);
		if(msg->result == 0)
			break;
	}
	return 0;
}

/**
 * @Function: createServerDir
 * @Description: 在服务器当前路径创建目录
 * @param socket_fd: 客户端的socket套接字
 * @param msg      : 服务器与客户端通信的数据结构体指针变量
 * @return  : 返回一个整数
 *            0,获取成功;
 *            -1,获取失败
 */
int createServerDir(int socket_fd, MSG * msg)
{
	msg->type = SERVER_MKDIR;/* 设置通信的消息类型为SERVER_MKDIR,表示请求在服务器当前路径新建目录 */
	
	char ch;
	/* 请输入目录名称 */
	bzero(msg->data, sizeof(msg->data));/* 先清空数据字符串空间 */
	printf("Please enter file name:");
	if( scanf("%s", msg->data) < 0)/* 获取输入数据 */
	{
		perror(RED"[error]scanf"CLS);
		return -1;
	}
	while ((ch = getchar()) != EOF && ch != '\n') ; /* 清除缓冲区的多余字符内容 */
	/* 发送数据 */
	if( send(socket_fd, msg, sizeof(MSG),0) < 0)
	{
		perror(RED"[error]send"CLS);
		return -1;
	}
	/* 等待服务器回传数据 */
	if( recv(socket_fd, msg, sizeof(MSG),0) < 0)
	{
		perror(RED"[error]recv"CLS);
		return -1;
	}
	printf(PURPLE"[server reply]:\n"CLS);
	if( msg-> result == 0)
	{
		printf(GREEN"[ OK  ]server create successfully!\n"CLS);
		printf("[new dir path]:%s\n", msg->data);
	}
	else
		printf(RED"[error]server failed to create directory(The directory may already exist)!\n"CLS);
	
	return 0;
}

/**
 * @Function: deleteServerFile
 * @Description: 在服务器当前路径删除文件
 * @param socket_fd: 客户端的socket套接字
 * @param msg      : 服务器与客户端通信的数据结构体指针变量
 * @return  : 返回一个整数
 *            0,获取成功;
 *            -1,获取失败
 */
int deleteServerFile(int socket_fd, MSG * msg)
{
	msg->type = SERVER_RM;/* 设置通信的消息类型为SERVER_RM,表示请求在服务器当前路径删除一个文件或目录 */

	char ch;
	char confirm;
	/* 请输入文件名称 */
	bzero(msg->data, sizeof(msg->data));/* 先清空数据字符串空间 */
	printf("Please enter file name:");
	if( scanf("%s", msg->data) < 0)/* 获取输入数据 */
	{
		perror(RED"[error]scanf"CLS);
		return -1;
	}
	while ((ch = getchar()) != EOF && ch != '\n') ; /* 清除缓冲区的多余字符内容 */
	printf(YELLOW"[warn ]The file name to be deleted is:[%s]\n"CLS, msg->data);
	printf("Are you sure to delete this file(Y or N):");
	if( scanf("%c", &confirm) < 0)/* 获取输入数据 */
	{
		perror(RED"[error]scanf"CLS);
		return -1;
	}
	while ((ch = getchar()) != EOF && ch != '\n') ; /* 清除缓冲区的多余字符内容 */
	if(confirm == 'N' || confirm == 'n')
	{
		printf(YELLOW"[warn ]Deletion of this file has been cancelled!\n"CLS);
		return -1;
	}
	/* 发送数据 */
	if( send(socket_fd, msg, sizeof(MSG),0) < 0)
	{
		perror(RED"[error]send"CLS);
		return -1;
	}
	/* 等待服务器回传数据 */
	if( recv(socket_fd, msg, sizeof(MSG),0) < 0)
	{
		perror(RED"[error]recv"CLS);
		return -1;
	}
	printf(PURPLE"[server reply]:\n"CLS);
	if( msg->result == 0)
	{
		printf(GREEN"[ OK  ]delete successfully!\n"CLS);
	}
	else
		printf(RED"[error]Failed to delete file(The directory may not exist)!\n"CLS);
	
	return 0;
}

/**
 * @Function: cdServerDir
 * @Description: 切换到指定的服务器目录
 * @param socket_fd: 客户端的socket套接字
 * @param msg      : 服务器与客户端通信的数据结构体指针变量
 * @return  : 返回一个整数
 *            0,获取成功;
 *            -1,获取失败
 */
int cdServerDir(int socket_fd, MSG * msg)
{
	char ch;
	msg->type = SERVER_CD;/* 设置通信的消息类型为SERVER_CD,表示请求切换服务器当前路径 */
	bzero(msg->data, sizeof(msg->data));/* 先清空数据字符串空间 */
	/* 输入 cd 命令 */
	printf("Please enter cd command(cd path):");
	if( scanf("%[^\n]", msg->data) < 0 )/* 获取输入数据，可以带空格 */
	{
		perror(RED"[error]gets"CLS);
		return -1;
	}
	while ((ch = getchar()) != EOF && ch != '\n') ; /* 清除缓冲区的多余字符内容 */
	
	/* 发送获取客户端IP和端口号的数据 */
	if( send(socket_fd, msg, sizeof(MSG),0) < 0)
	{
		perror(RED"[error]send"CLS);
		return -1;
	}
	/* 等待服务器回传数据 */
	if( recv(socket_fd, msg, sizeof(MSG),0) < 0)
	{
		perror(RED"[error]recv"CLS);
		return -1;
	}
	printf(PURPLE"[server reply]:"CLS"\n[server local PWD]:%s\n", msg->data);

	return 0;
}