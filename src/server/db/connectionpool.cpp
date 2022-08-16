#include <iostream>
#include <muduo/base/Logging.h>

#include "connectionpool.hpp"

shared_ptr<ConnectionPool> ConnectionPool::_pool = nullptr;

// 饿汉式单例模式接口
shared_ptr<ConnectionPool> ConnectionPool::get_connection_pool() {
	if (_pool == nullptr) {
		_pool = shared_ptr<ConnectionPool> (new ConnectionPool());// 堆区
	}
	return _pool;
}

// 连接池的构造
ConnectionPool::ConnectionPool() {
	_is_exit = false;

	// 加载配置项
	if (!load_config()) {
		return;
	}
	
	// 创建初始的连接
	for (int i = 0; i < _init_size; i++) {
		MySQL* p = new MySQL();    // 一条和数据库的连接
		p->connect(_ip, _port, _username, _password, _dbname);
		p->set_start_time();                 // 刷新空闲起始时间
		_connection_queue.push(p);
		_connection_cnt++;                   // 数据库连接的数量
	}

	// 启动一个线程，作为连接的生产者，底层调用C的pthread_create，没有对象的概念，需要绑定this指针
	thread producer(std::bind(&ConnectionPool::produce_conn_task, this)); // 给成员方法绑定当前对象 
	producer.detach();

	// 启动线程，扫描超过空闲时间_max_idle_time的连接进行回收
	thread scaner(std::bind(&ConnectionPool::scan_conn_task, this));
	scaner.detach();
}

ConnectionPool::~ConnectionPool(){
	unique_lock<mutex> lock(_queue_mutex);
	while(!_connection_queue.empty()){
		MySQL* conn = _connection_queue.front();
		_connection_queue.pop();
		conn->~MySQL();
	}
	_is_exit = true;
	_connection_cnt = 0;
	_cv.notify_all();     // 通知wait的线程起来退出
}

// 从配置文件中加载数据
bool ConnectionPool::load_config(){
	FILE* fp = fopen("/home/shen/chat/src/server/db/mysql_conn.conf", "r");
	if (fp == nullptr) {
		LOG_INFO << "open /home/shen/chat/src/server/db/mysql_conn.conf error!";
		return false;
	}

	while (!feof(fp)) {
		char line[1024] = { 0 };
		fgets(line, 1024, fp);
		string str(line);
		size_t start_idx = str.find('=', 0);  // 从0号位置开始查找
		if (start_idx == -1) {
			// 无效的配置项
			continue;
		}
		// ip=127.0.0.1
		size_t end_idx = str.find('\n', start_idx);
		string key = str.substr(0, start_idx);
		string value = str.substr(start_idx + 1, end_idx - start_idx - 1);
		// cout << key << " : " << value << endl;
		if (key == "ip") {
			_ip = value;
		}
		else if (key == "port") {
			_port = atoi(value.c_str());
		}
		else if (key == "username") {
			_username = value;
		}
		else if (key == "password") {
			_password = value;
		}
		else if (key == "dbname") {
			_dbname = value;
		}
		else if (key == "init_size") {
			_init_size = atoi(value.c_str());
		}
		else if (key == "max_size") {
			_max_size = atoi(value.c_str());
		}
		else if (key == "max_idle_time") {
			_max_idle_time = atoi(value.c_str());
		}
		else if (key == "connection_timeout") {
			_connection_timeout = atoi(value.c_str());
		}
	}
	return true;
}

// 运行在独立的线程中，负责生产新的连接
void ConnectionPool::produce_conn_task() {
	while (true) {
		// 生产者需要访问连接队列，加锁，防止和消费者同时访问
		unique_lock<mutex> lock(_queue_mutex);
		while (!_connection_queue.empty()) {
			// 连接队列不空（还有可以使用的连接），生产线程进入等待状态，并且释放互斥锁
			_cv.wait(lock);
		}

		// 队列空了，是因为析构函数执行了，就不再产生新的连接，直接退出
		if(_is_exit){
			LOG_INFO << "ConnectionPool::produce_conn_task thread exit";
			break;
		}
		
		// 只有当前连接小于_max_size才创建
		if (_connection_cnt < _max_size) {
			MySQL* p = new MySQL(); // 一条和数据库的连接
			p->connect(_ip, _port, _username, _password, _dbname);
			p->set_start_time();
			_connection_queue.push(p);
			_connection_cnt++;                   // 已和数据库连接的数量
		}

		// 通知消费者线程可以消费连接了
		_cv.notify_all();
	}
}

void ConnectionPool::scan_conn_task() {
	while (true) {
		// 定时检查队列超时的连接
		this_thread::sleep_for(chrono::seconds(_max_idle_time));
		if(_is_exit){
			// 线程睡醒了发现服务器已经退出了，当前线程也退出
			LOG_INFO << "ConnectionPool::scan_conn_task thread exit";
			break;
		}
		// 访问队列需要加锁
		unique_lock<mutex> lock(_queue_mutex);
		while (_connection_cnt > _init_size) {
			// 队列中的conn全都空闲，检查空闲的时间，超过_max_idle_time则释放
			MySQL* p = _connection_queue.front();
			if (p->get_free_time() >= (_max_idle_time * 1000)) {
				_connection_queue.pop();
				_connection_cnt--;
				delete p;
			}
			else {
				// 对头连接没有空闲超过_max_idle_time，其他连接肯定没有超过，本次检查结束
				break;
			}
		}
	}
}

// 给外部提供接口，从连接池中获取可用的空闲连接
// 通过智能指针自动管理外部的连接，不需要用户手动释放
shared_ptr<MySQL> ConnectionPool::get_connection() {
	unique_lock<mutex> lock(_queue_mutex);
	while (_connection_queue.empty()) {
		if (cv_status::timeout == _cv.wait_for(lock, chrono::milliseconds(_connection_timeout))) {
			if(_is_exit){
				LOG_INFO << "ConnectionPool::get_connection thread exit";
				break;
			}
			if (_connection_queue.empty()) {
				// 表示经过了_connection_timeout后超时醒来队列依然为空
				LOG_INFO << "获取连接超时...";
				return nullptr;
			}
		}
	}
	
	// 智能指针析构时，会调用对象的析构函数，把连接close
	// 需要自定义shared_ptr释放资源的方式，归还conn到queue

	// 从队列中获取连接
	shared_ptr<MySQL> p_conn(_connection_queue.front(), 
		[&](MySQL* p) {
			// 在多线程环境中调用，要考虑队列的线程安全
			unique_lock<mutex> lock(_queue_mutex);
			p->set_start_time();
			_connection_queue.push(p);
		});

		_connection_queue.pop();
		if (_connection_queue.empty()) {
			// 消费queue中最后一个连接，需要通知生产者生产
			_cv.notify_all();
		}
		return p_conn;
}
