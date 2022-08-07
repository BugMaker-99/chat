#include <muduo/base/Logging.h>
#include <vector>

#include "chatservice.hpp"
#include "public.hpp"

using namespace std;
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
    _msgHandlerMap.insert({ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this, _1, _2, _3)});
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
        response["id"] = user.getId();        // 注册成功后需要给用户返回id号，用于登录
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

            // 登录成功，查询当前用户是否有离线消息
            vector<string> vec = _offlineMsgModel.query(user.getId());
            if(!vec.empty()){
                // vec中存储在数据库表offlinemessage中查询的离线消息，用response返回给客户端，并删除数据库表中当前用户所有的离线消息
                response["offlinemsg"] = vec;
                _offlineMsgModel.remove(id);
            }
            // send返回给客户端
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

void ChatService::clientCloseException(const TcpConnectionPtr& conn){
    // 处理客户端没有发送json请求时的异常退出
    // 在_userConnMap中整表搜索，删除conn对应的键值对，并将数据库中conn对应的用户状态修改为offline

    // 可能有的用户正在登录，线程正在写_userConnMap，在多线程环境中需要保证_userConnMap的线程安全
    User user;
    {
        lock_guard<mutex> lock(_connMutex);
        for(auto it = _userConnMap.begin(); it != _userConnMap.end(); ++it){
            if(it->second == conn){
                // 找到异常退出的用户连接，并删除_userConnMap对应的键值对
                user.setId(it->first);
                _userConnMap.erase(it->first);
                break;
            }
        }
    }
    // 更新数据库中的state
    if(user.getId() != -1){
        // 在for循环中没找到conn对应的user，默认构造的user的id = -1
        user.setState("offline");
        _userModel.updateState(user);
    }
}

// 点对点聊天业务
void ChatService::oneChat(const TcpConnectionPtr& conn, const json& js, const Timestamp& time){
    int toid = js["to"].get<int>();
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(toid);
        if(it != _userConnMap.end()){
            // 接收者在线，需要进行转发
            it->second->send(js.dump());
            return;
        }
            
    }
    // 接收者不在线，需要存储离线消息
    _offlineMsgModel.insert(toid, js.dump());
}