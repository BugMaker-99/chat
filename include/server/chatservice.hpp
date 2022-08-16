#ifndef CHATSERVICE_H
#define CHATSERVICE_H

#include <muduo/net/TcpConnection.h>
#include <unordered_map>
#include <functional>
#include <mutex>

#include "redis.hpp"
#include "json.hpp"
#include "usermodel.hpp"
#include "friendmodel.hpp"
#include "groupmodel.hpp"
#include "offlinemessagemodel.hpp"
#include "connectionpool.hpp"

using namespace std;
using namespace muduo;
using namespace muduo::net;

using json = nlohmann::json;
using MsgHandler = std::function<void(const TcpConnectionPtr&, const json&, const Timestamp&)>;

// 聊天业务对象，有一个实例就够了，使用单例模式
class ChatService{
    public:
        // 获取单例对象的接口函数
        static shared_ptr<ChatService> getInstance();
        // 处理注册业务
        void reg(const TcpConnectionPtr& conn, const json& js, const Timestamp& time);
        // 处理登录业务
        void login(const TcpConnectionPtr& conn, const json& js, const Timestamp& time);
        //处理注销业务
        void logout(const TcpConnectionPtr &conn, const json& js, const Timestamp& time);

        // 点对点聊天业务
        void oneChat(const TcpConnectionPtr& conn, const json& js, const Timestamp& time);
        // 添加好友业务
        void addFriend(const TcpConnectionPtr& conn, const json& js, const Timestamp& time);

        // 创建群组业务
        void createGroup(const TcpConnectionPtr& conn, const json& js, const Timestamp& time);
        // 加入群组业务
        void joinGroup(const TcpConnectionPtr& conn, const json& js, const Timestamp& time);
        // 群组聊天业务
        void groupChat(const TcpConnectionPtr& conn, const json& js, const Timestamp& time);
        
        // 获取消息对应的回调函数
        MsgHandler getHandler(int msgid);

        // 处理客户端异常退出
        void clientCloseException(const TcpConnectionPtr& conn);
        // 处理服务器异常退出
        void reset();

        //从redis消息队列中获取订阅的消息
        void handleRedisSubscribeMessage(int userid, string msg);

    private:
        // 在构造函数里面把消息id对应的业务处理方法存放到_msgHandlerMap
        ChatService();

        static shared_ptr<ChatService> _service;

        // 存储消息id和其对应的业务处理方法
        unordered_map<int, MsgHandler> _msgHandlerMap;

        // 数据操作类对象
        UserModel _userModel;
        // 操作数据库存储离线消息对象
        OfflineMsgModel _offlineMsgModel;
        // 操作friend表的对象
        FriendModel _friendModel;
        // 操作allgroup和groupuser的对象
        GroupModel _groupModel;

        // 互斥锁保证_userConnMap的线程安全
        mutex _connMutex;
        // 存储在线用户的连接
        unordered_map<int, TcpConnectionPtr> _userConnMap;

        //redis操作对象
        Redis _redis;
};

#endif