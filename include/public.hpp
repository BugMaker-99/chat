#ifndef PUBLIC_H
#define PUBLIC_H

/*
    server和client的公共文件
*/

enum EnMsgType{
    LOGIN_MSG,                 // 登录消息
    LOGIN_MSG_ACK,
    REG_MSG,                  // 注册消息
    REG_MSG_ACK,              // 注册响应消息
    LOGOUT_MSG,               //注销消息

    ONE_CHAT_MSG,                 // 一对一聊天消息
    ADD_FRIEND_MSG,              // 添加好友消息

    CREATE_GROUP_MSG,          // 创建群组
    CREATE_GROUP_MSG_ACK,       // 创建群组响应
    JOIN_GROUP_MSG,              // 加入群组
    GROUP_CHAT_MSG,              // 群聊
};

#endif