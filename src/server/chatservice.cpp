#include <muduo/base/Logging.h>

#include "chatservice.hpp"
#include "public.hpp"

using namespace muduo;

ChatService* ChatService::instance(){
    // 线程安全的单例对象
    static ChatService service;
    return &service;
}

// 在构造函数里面把消息id对应的业务处理方法存放到_msgHandlerMap
ChatService::ChatService(){
    _msgHandlerMap.insert({LOGIN_MSG, std::bind(&ChatService::login, this, _1, _2, _3)});
    _msgHandlerMap.insert({REG_MSG, std::bind(&ChatService::reg, this, _1, _2, _3)});
}

MsgHandler ChatService::getHandler(int msgid){
    // 记录错误日志
    auto it = _msgHandlerMap.find(msgid);
    if(it == _msgHandlerMap.end()){
        // msgid没有对应的事件处理回调，返回默认的处理器
        return [=](const TcpConnectionPtr& conn, const json& js, const Timestamp& time){
            LOG_ERROR << "msgid:" << msgid << " can not find handler!";
        };
    }else{
        return _msgHandlerMap[msgid];
    }
}

// 处理注册业务
void ChatService::reg(const TcpConnectionPtr& conn, const json& js, const Timestamp& time){
    LOG_INFO<<"do reg service!!!";
}

// 处理登录业务
void ChatService::login(const TcpConnectionPtr& conn, const json& js, const Timestamp& time){
    LOG_INFO<<"do login service!!!";
}

