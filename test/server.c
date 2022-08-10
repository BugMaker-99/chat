#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>

int main(int argc, char **argv){
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1){
        printf("create socket failed!\n");
        return 0;
    }
    // 两个套接字地址
    struct sockaddr_in ser_addr, cli_addr;
    memset(&ser_addr, 0, sizeof(ser_addr));
    
    //解析通过命令行参数传递的ip和port
    char *ip = argv[1];
    uint16_t port = atoi(argv[2]);

    ser_addr.sin_family = AF_INET;    // 地址族
    ser_addr.sin_port = htons(port);  // host to net short
    ser_addr.sin_addr.s_addr = inet_addr(ip); // inet_addr：字符串转成整型的网络字节序点分十进制的ip地址

    // sockfd就是参观门口的服务员   ser_addr就是餐厅的地址     bind表示sockfd服务员为sockaddr餐厅工作
    // 给监听套接字指定ip port
    // sockfd表示手机    ser_addr是手机卡    ser_addr是卡大小   bind表示给手机插卡
    int res = bind(sockfd, (struct sockaddr*)&ser_addr, sizeof(ser_addr)); // struct sockaddr* 通用套接字地址结构
    if(res == -1){
        printf("bind failed!\n");
        return 0;
    }
    // 套接字 监听队列的大小
    listen(sockfd, 5);    // 创建监听队列（已完成三次握手的连接长度） 
    while(1){
        int len = sizeof(cli_addr);
        // accept从监听队列中取出连接，将信息取出来存入cli_addr。conn是文件描述符，就是餐馆内点菜的服务员
        int conn = accept(sockfd, (struct sockaddr*)&cli_addr, &len); 
        if(conn < 0){
            // 取出连接失败
            continue;
        }
        printf("accept conn = %d\n", conn);
        while(1){
            char buff[128] = {0};
            // ssize_t recv(int sockfd, void *buf, size_t len, int flags);
            int n = recv(conn, buff, 127, 0); // client断开连接，就会返回0
            if(n == 0){
                printf("client %d exit\n", conn);
                break;
            }
            printf("buff(%d) = %s\n", n, buff);
            // ssize_t send(int sockfd, const void *buf, size_t len, int flags);
            send(conn, "welcome connect server!\n", sizeof("welcome connect server!\n"), 0);
        }
        close(conn);
    }
    return 0;
}
