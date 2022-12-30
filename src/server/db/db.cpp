#include <muduo/base/Logging.h>

#include "db.hpp"

// 数据库配置信息
// static string server = "127.0.0.1";
// static string user = "root";
// static string password = "123456";
// static string dbname = "chat";

//初始化数据库连接
MySQL::MySQL(){
    // 实际上是malloc了一个MYSQL结构体大小的指针
    _conn = mysql_init(nullptr); 
}

//释放数据库连接资源
MySQL::~MySQL(){
    if (_conn != nullptr){
        mysql_close(_conn);
        LOG_INFO << "a MYSQL connection closed";
    }
}

//连接数据库
bool MySQL::connect(string ip, unsigned short port, string username, string password, string dbname){
    MYSQL *p = mysql_real_connect(_conn, ip.c_str(), username.c_str(),
                        password.c_str(), dbname.c_str(), port, nullptr, 0);
    if (p != nullptr){
        //C和C++代码默认的编码字符是ASCII，如果不设置，从MySQL上拉下来的中文显示？
        mysql_query(_conn, "set names gbk");
        LOG_INFO << "connect mysql success!";
    }
    else{
        LOG_INFO << "connect mysql fail!";
    }

    return p;
}

//更新操作
bool MySQL::update(string sql){
    if (mysql_query(_conn, sql.c_str())){
        LOG_INFO << __FILE__ << ":" << __LINE__ << ":"
                 << sql << "更新失败!";
        return false;
    }

    return true;
}

//查询操作
MYSQL_RES* MySQL::query(string sql){
    if (mysql_query(_conn, sql.c_str())){
        LOG_INFO << __FILE__ << ":" << __LINE__ << ":"
                 << sql << "查询失败!";
        return nullptr;
    }
    
    return mysql_use_result(_conn);
}

//获取连接
MYSQL* MySQL::getConnection(){
    return _conn;
}
