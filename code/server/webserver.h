#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <unordered_map>
#include <fcntl.h>       // fcntl()
#include <unistd.h>      // close()
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "epoller.h"
#include "../log/log.h"
#include "../timer/heaptimer.h"
#include "../pool/sqlconnpool.h"
#include "../pool/threadpool.h"
#include "../pool/sqlconnRAII.h"
#include "../http/httpconn.h"

//定义了WebServer类,该类用于构建WebServer。使用Epoller来监听新连接
//并使用回调函数来创建HttpConn对象来处理连接的读写事件,最后使用线程池将
//客户端任务与数据库任务加入工作队列中进行后续处理
class WebServer {
public:
    WebServer(
            int port, int trigMode, int timeoutMS, bool OptLinger,
            int sqlPort, const char *sqlUser, const char *sqlPwd,
            const char *dbName, int connPoolNum, int threadNum,
            bool openLog, int logLevel, int logQueSize);

    ~WebServer();

    void Start();

private:
    bool InitSocket_();

    void InitEventMode_(int trigMode);

    void AddClient_(int fd, sockaddr_in addr);

    void DealListen_();

    void DealWrite_(HttpConn *client);

    void DealRead_(HttpConn *client);

    void SendError_(int fd, const char *info);

    void ExtentTime_(HttpConn *client);

    void CloseConn_(HttpConn *client);

    void OnRead_(HttpConn *client);

    void OnWrite_(HttpConn *client);

    void OnProcess(HttpConn *client);

    static const int MAX_FD = 65536;

    static int SetFdNonblock(int fd);

    int port_;        //表示服务器监听的端口号
    bool openLinger_; //表示是否开启优雅关闭连接
    int timeoutMS_;   //表示客户端连接的超时时间（毫秒）
    bool isClose_;    //表示服务器是否关闭
    int listenFd_;    //表示服务器监听 socket 的文件描述符
    char *srcDir_;    //表示服务器资源目录

    uint32_t listenEvent_;  //表示 epoll 监听的事件类型
    uint32_t connEvent_;    //表示客户端连接的事件类型

    std::unique_ptr <HeapTimer> timer_;         //表示 Web 服务器使用的定时器，用于超时检测
    std::unique_ptr <ThreadPool> threadpool_;   //表示 Web 服务器使用的线程池，用于处理客户端请求
    std::unique_ptr <Epoller> epoller_;         //表示 Web 服务器使用的 epoll 实例，用于监听和处理事件
    std::unordered_map<int, HttpConn> users_;   //表示当前所有的客户端连接，其中键为文件描述符，值为 HttpConn 类型的对象
};


#endif //WEBSERVER_H