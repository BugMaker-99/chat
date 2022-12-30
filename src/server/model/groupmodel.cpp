#include "connectionpool.hpp"
#include "groupmodel.hpp"

// 创建群组
bool GroupModel::createGroup(Group& group){
    char sql[1024] = {0};
    sprintf(sql, "insert into allgroup(groupname, groupdesc) values('%s', '%s')", 
            group.getName().c_str(), group.getDesc().c_str());
    
    shared_ptr<MySQL> conn_ptr = ConnectionPool::get_connection_pool()->get_connection();
    if(conn_ptr->update(sql)){
        // 组名不允许重复，若重复，则update返回false，群组创建失败
        // 若群组创建成功，则把群id带回，调用网络接口发送给用户
        group.setId(mysql_insert_id(conn_ptr->getConnection()));
        return true;
    }

    return false;
}

bool GroupModel::isGroupExist(int groupid){
    // 1. 组装SQL语句
    char sql[1024] = {0};
    // 此处刚注册，还没有登录，肯定是offline，我们在User类的构造函数里state默认为offline
    sprintf(sql, "select count(id) from allgroup where id = %d;", groupid);
    
    // 2. 使用MySQL对象操作数据库
    shared_ptr<MySQL> conn_ptr = ConnectionPool::get_connection_pool()->get_connection();
    MYSQL_RES* res = conn_ptr->query(sql);
    if(res != nullptr){
        MYSQL_ROW row = mysql_fetch_row(res);
        if(row != nullptr && atoi(row[0]) == 0){
            // 群组不存在
            return false;
        }
    }
    return true;
}

// userid用户加入群组
void GroupModel::joinGroup(int userid, int groupid, string role){
    char sql[1024] = {0};
    sprintf(sql, "insert into groupuser values(%d, %d, '%s')",
            groupid, userid, role.c_str());
    
    shared_ptr<MySQL> conn_ptr = ConnectionPool::get_connection_pool()->get_connection();
    conn_ptr->update(sql);
}

// 查询userid用户所在所有群组的信息
vector<Group> GroupModel::queryGroups(int userid){
    /*
        1. 先根据userid在groupuser表中查出该用户所属群组的信息
        2. 再根据群组信息，查询属于该群组的所有用户的userid，并且和user表进行联合查询，查出用户的详细信息
    */

    // 1. 先根据userid在groupuser表中查出该用户加入了哪些群
    char sql[1024] = {0};
    sprintf(sql, "select a.id, a.groupname, a.groupdesc from allgroup a inner join groupuser b on a.id=b.groupid where b.userid=%d", userid);
    
    vector<Group> groupVec;
    shared_ptr<MySQL> conn_ptr = ConnectionPool::get_connection_pool()->get_connection();
    // malloc申请了资源，需要释放
    MYSQL_RES* res = conn_ptr->query(sql);
    if(res != nullptr){
        MYSQL_ROW row;
        Group group;
        while((row = mysql_fetch_row(res)) != nullptr){
            group.setId(atoi(row[0]));
            group.setName(row[1]);
            group.setDesc(row[2]);
            groupVec.push_back(group);
        }
        // 释放结果集占用的资源
        mysql_free_result(res);
    }

    // 2. 再根据群组信息，查询属于该群组的所有用户的userid，并且和user表进行联合查询，查出用户的详细信息
    for(Group& group : groupVec){
        sprintf(sql, "select a.id, a.name, a.state, b.grouprole from user a inner join groupuser b on a.id=b.userid where b.groupid=%d", group.getId());

        MYSQL_RES* res = conn_ptr->query(sql);
        if(res != nullptr){
            MYSQL_ROW row;
            GroupUser guser;
            while((row = mysql_fetch_row(res)) != nullptr){
                guser.setId(atoi(row[0]));
                guser.setName(row[1]);
                guser.setState(row[2]);
                guser.setRole(row[3]);
                group.getUsers().push_back(guser);
            }
            // 释放结果集占用的资源
            mysql_free_result(res);
        }
    }

    return groupVec;
}

// 根据groupid查询除userid自己以外的其他群组用户id列表，主要用于群聊业务给群组其他成员群发消息
vector<int> GroupModel::queryGroupUsers(int userid, int groupid){
    char sql[1024] = {0};
    // 在groupuser中查询群号为groupid的用户id，并除开自己的userid（因为群聊时，不需要给自己转发消息，给群里其他成员转发即可）
    sprintf(sql, "select userid from groupuser where groupid=%d and userid!=%d", groupid, userid);
    
    vector<int> useridVec;
    shared_ptr<MySQL> conn_ptr = ConnectionPool::get_connection_pool()->get_connection();
    // malloc申请了资源，需要释放
    MYSQL_RES* res = conn_ptr->query(sql);
    if(res != nullptr){
        MYSQL_ROW row;
        while((row = mysql_fetch_row(res)) != nullptr){
            useridVec.push_back(atoi(row[0]));
        }
        // 释放结果集占用的资源
        mysql_free_result(res);
    }

    return useridVec;
}
