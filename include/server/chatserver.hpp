#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <iostream>

using namespace muduo;
using namespace muduo::net;
using namespace std;

class ChatServer{
    public:
        // 初始化聊天服务器对象
        ChatServer(EventLoop *loop, const InetAddress &listenAddr, const string &nameArg);

        // 启动服务器
        void start();

    private:
        // 上报连接相关信息的回调函数
        void onConnection(const TcpConnectionPtr &conn);

        // 上报读写事件相关信息的回调函数
        void onMessage(const TcpConnectionPtr &conn, Buffer *buffer, Timestamp time);

        // muduo库中实现服务器功能的对象
        TcpServer _server;
        // 指向事件循环对象的指针
        EventLoop* _loop;
};

#endif