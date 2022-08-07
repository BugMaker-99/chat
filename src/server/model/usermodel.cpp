#include "usermodel.hpp"
#include "db.hpp"

// User表的增加方法，用户注册时需要使用
bool UserModel::insert(User& user){
    // 1. 组装SQL语句
    char sql[1024] = {0};
    // 此处刚注册，还没有登录，肯定是offline，我们在User类的构造函数里state默认为offline
    sprintf(sql, "insert into User(name, password, state) values('%s', '%s', '%s')",
        user.getName().c_str(), user.getPwd().c_str(), user.getState().c_str());
    
    // 2. 使用MySQL对象操作数据库
    MySQL mysql;
    if(mysql.connect()){
        if(mysql.update(sql)){
            // 用户注册成功，则将用户对应的主键id添加到user对象
            int insert_id = mysql_insert_id(mysql.getConnection());
            user.setId(insert_id);
            return true;
        }
    }

    return false;
}
