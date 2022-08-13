#ifndef DB_H
#define DB_H

#include <mysql/mysql.h>//mysql的头文件 
#include <string>
#include <ctime>

using namespace std;

//数据库操作类
class MySQL{
    public:
        //初始化数据库连接
        MySQL();
        //释放数据库连接资源
        ~MySQL();
        //连接数据库
        bool connect(string ip, unsigned short port, string username, string password, string dbname);
        //更新操作
        bool update(string sql);
        //查询操作
        MYSQL_RES *query(string sql);
        //获取连接
        MYSQL* getConnection();
        // 刷新连接的起始空闲时间点
        void set_start_time() { _free_start_time = clock(); }
        // 返回空闲状态存活的时间
        clock_t get_free_time() { return clock() - _free_start_time; }
        
    private:
        MYSQL *_conn;//连接 
        clock_t _free_start_time;        // 记录进入空闲状态后的起始时间
};

#endif
