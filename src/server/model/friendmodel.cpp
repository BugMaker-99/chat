#include "friendmodel.hpp"
#include "connectionpool.hpp"

// 添加好友关系，需要写friend表
void FriendModel::insert(int userid, int friendid){
    // 1. 组装SQL语句
    char sql[1024] = {0};
    // 此处刚注册，还没有登录，肯定是offline，我们在User类的构造函数里state默认为offline
    sprintf(sql, "insert into friend values(%d, %d)", userid, friendid);
    
    // 2. 使用MySQL对象操作数据库
    shared_ptr<MySQL> conn_ptr = ConnectionPool::get_connection_pool()->get_connection();
    conn_ptr->update(sql);
    
}

// 返回用户好友列表，通过userid在friend表找到好友，再到user表中找到用户信息，并返回给用户
vector<User> FriendModel::query(int userid){
    // 1. 组装SQL语句
    char sql[1024] = {0};
    // 此处刚注册，还没有登录，肯定是offline，我们在User类的构造函数里state默认为offline
    sprintf(sql, "select user.id, user.name, user.state from user inner join friend on user.id=friend.friendid where friend.userid=%d", userid);
    
    // 2. 使用MySQL对象操作数据库
    vector<User> vec;
    shared_ptr<MySQL> conn_ptr = ConnectionPool::get_connection_pool()->get_connection();
        // malloc申请了资源，需要释放
        MYSQL_RES* res = conn_ptr->query(sql);
        if(res != nullptr){
            MYSQL_ROW row;
            while((row = mysql_fetch_row(res)) != nullptr){
                User user;
                user.setId(atoi(row[0]));
                user.setName(row[1]);
                user.setState(row[2]);
                vec.push_back(user);
            }
            // 释放结果集占用的资源
            mysql_free_result(res);
        }
    
    // 如果数据库没有连接上，或者没查找到id对应数据，则返回默认构造的user，id=-1
    return vec;
}