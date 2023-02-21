#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <sys/types.h>
#include <sys/uio.h>     // readv/writev
#include <arpa/inet.h>   // sockaddr_in
#include <stdlib.h>      // atoi()
#include <errno.h>

#include "../log/log.h"
#include "../pool/sqlconnRAII.h"
#include "../buffer/buffer.h"
#include "httprequest.h"
#include "httpresponse.h"

class HttpConn {
public:
    HttpConn();

    ~HttpConn();

    void init(int sockFd, const sockaddr_in &addr);

    ssize_t read(int *saveErrno);

    ssize_t write(int *saveErrno);

    void Close();

    int GetFd() const;

    int GetPort() const;

    const char *GetIP() const;

    sockaddr_in GetAddr() const;

    bool process();

    /**
     * @brief 返回写缓冲区中的字节数
     * @return
     */
    int ToWriteBytes() {
        return iov_[0].iov_len + iov_[1].iov_len;
    }

    /**
     * @brief 判断连接是否为长连接
     * @return
     */
    bool IsKeepAlive() const {
        return request_.IsKeepAlive();
    }

    static bool isET;                   //标记是否采用 ET 模式，即边缘触发模式
    static const char *srcDir;          //HTTP 服务器的根目录
    static std::atomic<int> userCount;  //当前连接的 HTTP 客户端数目的原子变量

private:

    int fd_;                    //HTTP 连接使用的文件描述符
    struct sockaddr_in addr_;   //连接的客户端 IP 和端口号

    bool isClose_;              //标记连接是否关闭

    int iovCnt_;                //结构体数组的元素个数
    struct iovec iov_[2];       //结构体数组，用于将数据从缓冲区读到文件描述符或者从文件描述符写入缓冲区

    Buffer readBuff_; // 读缓冲区，用于存储从文件描述符读取到的数据
    Buffer writeBuff_; // 写缓冲区，用于存储需要写入文件描述符的数据

    HttpRequest request_;       //HttpRequest 类的对象，存储从客户端接收到的 HTTP 请求
    HttpResponse response_;     //HttpResponse 类的对象，用于生成 HTTP 响应
};


#endif //HTTP_CONN_H