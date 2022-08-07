#ifndef USERMODEL_H
#define USERMODEL_H

#include "user.hpp"

// User表的数据操作类，和业务不相关，针对表的CRUD
class UserModel{
    public:
        // User表的增加方法
        bool insert(User& user);

        // 根据用户id查询用户信息
        User query(int id);

        // 登录成功后，更新数据库中的用户在线状态
        bool updateState(User user);
};

#endif