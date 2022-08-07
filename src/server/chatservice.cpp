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

// 处理注册业务  name  password
void ChatService::reg(const TcpConnectionPtr& conn, const json& js, const Timestamp& time){
    // 网络模块把数据进行序列化，生成json对象，传到业务层
    string name = js["name"];
    string pwd = js["password"];

    User user;
    user.setName(name);
    user.setPwd(pwd);

    // 直接用_userModel对象操作数据库，业务层看不见SQL语句
    bool is_succeed = _userModel.insert(user);
    json response;
    if(is_succeed){
        // 注册成功
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 0;                // 0表示成功
        response["id"] = user.getId();
        conn->send(response.dump());
    }else{
        // 注册失败
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 1;                // 1表示失败
        conn->send(response.dump());
    }
}

// 处理登录业务 id  pwd
void ChatService::login(const TcpConnectionPtr& conn, const json& js, const Timestamp& time){
    // js是用户输入的登录数据，用id和password登录
    int id = js["id"].get<int>();
    string pwd = js["password"];

    // 用id在数据库中查找对应的一行数据
    User user = _userModel.query(id);
    json response;
    if(user.getId() == id && user.getPwd() == pwd){
        if(user.getState() == "online"){
            // 用户已经登录，不允许重复登录
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2;                // 2表示重复登录
            response["errmsg"] = "该账号已经登录，无法重复登录"; 
            conn->send(response.dump());
        }else{
            // 查找到id对应的用户，且密码匹配，则登录成功
            {
                // 仅仅保证_userConnMap的线程安全即可，锁的粒度尽可能小
                lock_guard<mutex> lock(_connMutex);
                _userConnMap.insert({id, conn});
            }

            // 登录成功后，需要更新数据库中用户的在线信息
            user.setState("online");
            _userModel.updateState(user);

            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0;                // 0表示成功
            response["id"] = user.getId();
            response["name"] = user.getName();
            conn->send(response.dump());
        }
    }else{
        // 登录失败
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 1;                // 1表示失败
        response["errmsg"] = "用户名或密码错误"; 
        conn->send(response.dump());
    }
}
