#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<assert.h>

int main(){
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1){
        printf("create socket failed!\n");
        return 0;
    }
    struct sockaddr_in cli_addr;
    memset(&cli_addr, 0, sizeof(cli_addr));

    cli_addr.sin_family = AF_INET;    // 地址族
    cli_addr.sin_port = htons(8888);  // host to net short
    cli_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // inet_addr将字符串转为无符号整型

    // 可以将套接字绑定ip，但一般客户端不绑定，让OS随机分配port
    int res = connect(sockfd, (struct sockaddr*)&cli_addr, sizeof(cli_addr));  // 连接server
    assert(res != -1);

    while(1){
        char buff[128] = {0};
        printf("input:");
        fgets(buff, 128, stdin);
        if(strcmp(buff, "exit\n") == 0){
            break;
        }
        send(sockfd, buff, strlen(buff), 0);
        memset(buff, 0 ,128);
        int n = recv(sockfd, buff, 127, 0);
        printf("buff(%d) = %s", n, buff);
    }

    close(sockfd);
    return 0;
}
