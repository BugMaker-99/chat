基于moduo实现的集群聊天服务器，用nginx实现负载均衡，用redis解耦服务器的硬连接，通过消息队列发布订阅的方式实现跨服务器聊天

编译方式：
cd build
rm -rf *
cmake ..
make
