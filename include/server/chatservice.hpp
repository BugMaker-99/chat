#ifndef CHATSERVICE_H
#define CHATSERVICE_H

#include <muduo/net/TcpConnection.h>
#include <unordered_map>
#include <functional>

#include "json.hpp"
#include "usermodel.hpp"

using namespace std;
using namespace muduo;
using namespace muduo::net;

using json = nlohmann::json;
using MsgHandler = std::function<void(const TcpConnectionPtr&, const json&, const Timestamp&)>;

// 聊天业务对象，有一个实例就够了，使用单例模式
class ChatService{
    public:
        // 获取单例对象的接口函数
        static ChatService* instance();
        // 处理注册业务
        void reg(const TcpConnectionPtr& conn, const json& js, const Timestamp& time);
        // 处理登录业务
        void login(const TcpConnectionPtr& conn, const json& js, const Timestamp& time);
        // 获取消息对应的回调函数
        MsgHandler getHandler(int msgid);

    private:
        // 在构造函数里面把消息id对应的业务处理方法存放到_msgHandlerMap
        ChatService();
        // 存储消息id和其对应的业务处理方法
        unordered_map<int, MsgHandler> _msgHandlerMap;

        // 数据操作类
        UserModel _userModel;
};

#endif