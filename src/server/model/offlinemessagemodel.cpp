#include "offlinemessagemodel.hpp"
#include "db.hpp"

// 存储用户的离线消息
void OfflineMsgModel::insert(int userid, string msg){
    // 1. 组装SQL语句
    char sql[1024] = {0};
    // 此处刚注册，还没有登录，肯定是offline，我们在User类的构造函数里state默认为offline
    sprintf(sql, "insert into offlinemessage values(%d, '%s')", userid, msg.c_str());

    // 2. 使用MySQL对象操作数据库
    MySQL mysql;
    if(mysql.connect()){
        mysql.update(sql);
    }
}

// 删除用户的离线消息
void OfflineMsgModel::remove(int userid){
    // 1. 组装SQL语句
    char sql[1024] = {0};
    // 此处刚注册，还没有登录，肯定是offline，我们在User类的构造函数里state默认为offline
    sprintf(sql, "delete from offlinemessage where userid=%d", userid);
    
    // 2. 使用MySQL对象操作数据库
    MySQL mysql;
    if(mysql.connect()){
        mysql.update(sql);
    }
}

// 查询用户的离线消息
vector<string> OfflineMsgModel::query(int userid){
    // 1. 组装SQL语句
    char sql[1024] = {0};
    // 此处刚注册，还没有登录，肯定是offline，我们在User类的构造函数里state默认为offline
    sprintf(sql, "select message from offlinemessage where userid=%d", userid);
    
    // 2. 使用MySQL对象操作数据库
    vector<string> vec;
    MySQL mysql;
    if(mysql.connect()){
        // malloc申请了资源，需要释放
        MYSQL_RES* res = mysql.query(sql);
        if(res != nullptr){
            MYSQL_ROW row;
            while((row = mysql_fetch_row(res)) != nullptr){
                vec.push_back(row[0]);
            }
            // 释放结果集占用的资源
            mysql_free_result(res);
        }
    }
    // 如果数据库没有连接上，或者没查找到id对应数据，则返回默认构造的user，id=-1
    return vec;
}