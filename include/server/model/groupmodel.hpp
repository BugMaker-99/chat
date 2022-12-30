#ifndef GROUPMODEL_H
#define GROUPMODEL_H

#include "group.hpp"
#include <vector>
#include <string>

using namespace std;

class GroupModel{
    public:
        // 创建群组
        bool createGroup(Group& group);
        // 查询群组是否存在
        bool isGroupExist(int groupid);
        // userid用户加入群组
        void joinGroup(int userid, int groupid, string role);
        // 查询userid用户所在所有群组信息
        vector<Group> queryGroups(int userid);
        // 根据groupid查询除userid自己以外的其他群组用户id列表，主要用于群聊业务给群组其他成员群发消息
        vector<int> queryGroupUsers(int userid, int groupid);
};


#endif