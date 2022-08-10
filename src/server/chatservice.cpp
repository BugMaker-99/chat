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
    _msgHandlerMap.insert({LOGOUT_MSG, std::bind(&ChatService::logout, this, _1, _2, _3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG, std::bind(&ChatService::addFriend, this, _1, _2, _3)});
    
    // 群组业务
    _msgHandlerMap.insert({CREATE_GROUP_MSG, std::bind(&ChatService::createGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({JOIN_GROUP_MSG, std::bind(&ChatService::joinGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});

    if(_redis.connect()){
        // 连接成功后，注册处理redis上报的消息的回调函数
        _redis.init_notify_handler(std::bind(&ChatService::handleRedisSubscribeMessage, this, _1, _2));
    }
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
            response["errmsg"] = "this account is using! can not login again!"; 
            conn->send(response.dump());
        }else{
            // 查找到id对应的用户，且密码匹配，则登录成功
            {
                // 仅仅保证_userConnMap的线程安全即可，锁的粒度尽可能小
                lock_guard<mutex> lock(_connMutex);
                _userConnMap.insert({id, conn});
            }

            // id用户登录后，需要向redis订阅关于id的消息
            _redis.subscribe(id);

            // 登录成功后，需要更新数据库中用户的在线信息
            user.setState("online");
            _userModel.updateState(user);

            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0;                // 0表示成功
            response["id"] = user.getId();
            response["name"] = user.getName();

            // 登录成功，查询当前用户是否有离线消息
            vector<string> msgVec = _offlineMsgModel.query(id);
            if(!msgVec.empty()){
                // vec中存储在数据库表offlinemessage中查询的离线消息，用response返回给客户端，并删除数据库表中当前用户所有的离线消息
                response["offlinemsg"] = msgVec;
                _offlineMsgModel.remove(id);
            }
            // 查询该用户的好友信息并返回
            // 将每个好友的信息打包成json字符串，用infoVec保存
            vector<User> userVec = _friendModel.query(id);
            if(!userVec.empty()){
                vector<string> infoVec;
                for(User& user : userVec){
                    json js;
                    js["id"] = user.getId();
                    js["name"] = user.getName();
                    js["state"] = user.getState();
                    infoVec.push_back(js.dump());
                }
                response["friends"] = infoVec;
            }
            // 获取当前用户id所在群组信息
            vector<Group> groupUserVec = _groupModel.queryGroups(id);
            if(!groupUserVec.empty()){
                // group:[{groupid:[xxx, xxx, xxx, xxx]}]
                vector<string> groupVec;
                // 开始打包群组信息
                for(Group& group : groupUserVec){
                    // 一个groupJson包括群id、群名和描述信息，以及所有的成员信息
                    json groupJson;
                    groupJson["groupid"] = group.getId();
                    groupJson["groupname"] = group.getName();
                    groupJson["groupdesc"] = group.getDesc();

                    // userVec每个元素就是一个组员的信息，一个userVec就是一个群所有的成员的信息
                    vector<string> userVec;
                    for(GroupUser& guser : group.getUsers()){
                        // 一个guserJson封装一个群成员的信息
                        json guserJson;
                        guserJson["id"] = guser.getId();
                        guserJson["name"] = guser.getName();
                        guserJson["state"] = guser.getState();
                        guserJson["role"] = guser.getRole();
                        userVec.push_back(guserJson.dump());
                    }

                    groupJson["users"] = userVec;
                    groupVec.push_back(groupJson.dump());
                }

                response["groups"] = groupVec;
            }
            // send返回给客户端
            conn->send(response.dump());
        }
    }else{
        // 登录失败
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 1;                // 1表示失败
        response["errmsg"] = "id or password error!"; 
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

    // 用户异常退出，在redis中取消当前服务器订阅的通道
    _redis.unsubscribe(user.getId());

    // 更新数据库中的state
    if(user.getId() != -1){
        // 在for循环中没找到conn对应的user，默认构造的user的id = -1
        user.setState("offline");
        _userModel.updateState(user);
    }
}

// 处理退出业务
void ChatService::logout(const TcpConnectionPtr &conn, const json& js, const Timestamp& time){
    int userid = js["id"].get<int>();

    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(userid);
        if (it != _userConnMap.end()){
            _userConnMap.erase(it);
        }
    }

    // 用户退出，在redis中取消当前服务器订阅的通道
    _redis.unsubscribe(userid);

    // 更新用户的状态信息
    User user(userid, "", "", "offline");
    _userModel.updateState(user);
}

// 处理服务器异常退出，服务器退出后，需要把数据库中所有的用户state都设置为offline
void ChatService::reset(){
    _userModel.resetState();
}

// 点对点聊天业务
void ChatService::oneChat(const TcpConnectionPtr& conn, const json& js, const Timestamp& time){
    int toid = js["toid"].get<int>();
    
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(toid);
        if(it != _userConnMap.end()){
            // 接收者在线，需要进行转发
            it->second->send(js.dump());
            return;
        }
            
    }
    User user = _userModel.query(toid);
    if(user.getState() == "online"){
        // toid在其他服务器
        _redis.publish(toid, js.dump());
    }else{
        // 接收者不在线，需要存储离线消息
        _offlineMsgModel.insert(toid, js.dump());
    }
}

// 添加好友业务 msgid  id  friendid
void ChatService::addFriend(const TcpConnectionPtr& conn, const json& js, const Timestamp& time){
    int userid = js["userid"].get<int>();
    int friendid = js["friendid"].get<int>();

    _friendModel.insert(userid, friendid);
    _friendModel.insert(friendid, userid);
}

// 创建群组业务
void ChatService::createGroup(const TcpConnectionPtr& conn, const json& js, const Timestamp& time){
    int userid = js["id"].get<int>();
    string name = js["groupname"];
    string desc = js["groupdesc"];

    // 存储新创建的群组信息，群id初始化为-1
    Group group(-1, name, desc);
    json response;
    // 访问数据库的allgroup表，插入群组信息
    if(_groupModel.createGroup(group)){
        // 群名重复时，则创建失败
        // 创建成功，则访问groupuser表，将当前userid设置为creator
        _groupModel.joinGroup(userid, group.getId(), "creator");
        response["msgid"] = CREATE_GROUP_MSG_ACK;
        response["errno"] = 0;
        response["groupid"] = group.getId();
        conn->send(response.dump());
    }else{
        // 创建失败
        response["msgid"] = CREATE_GROUP_MSG_ACK;
        response["errno"] = 1;
        response["groupname"] = name;
        conn->send(response.dump());
    }
}

// 加入群组业务
void ChatService::joinGroup(const TcpConnectionPtr& conn, const json& js, const Timestamp& time){
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    // 数据库层访问groupuser，一个用户加入群，添加一条记录
    _groupModel.joinGroup(userid, groupid, "normal");
}

// 群组聊天业务
void ChatService::groupChat(const TcpConnectionPtr& conn, const json& js, const Timestamp& time){
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();

    // 查询groupid中除了userid以外的其他成员
    vector<int> useridVec = _groupModel.queryGroupUsers(userid, groupid);

    // 读写_userConnMap时，需要保证线程安全
    lock_guard<mutex> lock(_connMutex);
    // 遍历所有的组员，若在线则转发消息，不在线则保存至离线消息表
    for(int toid : useridVec){
        auto it = _userConnMap.find(toid);
        if(it != _userConnMap.end()){
            // 找出连接，直接转发json
            it->second->send(js.dump());
        }else{
            User user = _userModel.query(toid);
            if(user.getState() == "online"){
                // toid在其他服务器
                _redis.publish(toid, js.dump());
            }else{
                // 接收者不在线，需要存储离线消息
                _offlineMsgModel.insert(toid, js.dump());
            }
        }
    }
}

// redis上报toid和msg，当前服务器用handleRedisSubscribeMessage处理
void ChatService::handleRedisSubscribeMessage(int toid, string msg){
    lock_guard<mutex> lock(_connMutex);
    // 因为是其他服务器发现toid在线，但是不在那台服务器上，才会将消息publish到消息队列
    // 然后当前服务器才能执行handleRedisSubscribeMessage拿到消息，一般情况下，toid都是在线的
    auto it = _userConnMap.find(toid);
    if (it != _userConnMap.end()){
        it->second->send(msg);
    }else{
        // 也许就是其他服务器publish后，当前服务器拿到msg前，toid下线了
        // 才可能需要存储该用户的离线消息
        _offlineMsgModel.insert(toid, msg);
    }
}