#include "chatserver.hpp"
#include "chatservice.hpp"
#include "connectionpool.hpp"

#include <iostream>
#include <signal.h>

using namespace std;

// 处理服务器ctrl c结束的方法
void resetHandler(int){
    ChatService::getInstance()->reset();
    // delete new出来的单例
    delete ChatService::getInstance();
    delete ConnectionPool::get_connection_pool();
    
    // exit后，所有的.stack段变量被回收、.data段的单例对象会出作用域执行析构
    exit(0);
}

int main(int argc, char **argv){
    if (argc < 3)    {
        cerr << "command invalid! example: ./ChatServer 127.0.0.1 6000" << endl;
        exit(-1);
    }

    //解析通过命令行参数传递的ip和port
    char *ip = argv[1];
    uint16_t port = atoi(argv[2]);

    // 注册信号处理函数
    signal(SIGINT, resetHandler);

    EventLoop loop;
    InetAddress addr(ip, port);
    ChatServer server(&loop, addr, "ChatServer");
    
    server.start();
    loop.loop();

    return 0;
}
