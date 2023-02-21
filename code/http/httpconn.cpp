#include "httpconn.h"

using namespace std;

const char *HttpConn::srcDir;
std::atomic<int> HttpConn::userCount;
bool HttpConn::isET;

/**
 * 构造函数
 */
HttpConn::HttpConn() {
    fd_ = -1;       //Socket文件描述符
    addr_ = {0};    //客户端地址信息
    isClose_ = true;//连接是否关闭
};

/**
 * 析构函数
 */
HttpConn::~HttpConn() {
    Close();
};

/**
 * @brief 初始化一个HttpConn对象
 * @param fd 文件描述符
 * @param addr 客户端地址信息
 */
void HttpConn::init(int fd, const sockaddr_in &addr) {
    assert(fd > 0);
    userCount++;    //客户端连接数加一
    addr_ = addr;   //
    fd_ = fd;
    //读写缓冲区使用RetrieveAll函数进行清空
    //确保之前缓存的数据不会对新的连接产生影响
    writeBuff_.RetrieveAll();
    readBuff_.RetrieveAll();
    isClose_ = false;//未关闭连接
    //确保之前缓存的数据不会对新的连接产生影响
    LOG_INFO("Client[%d](%s:%d) in, userCount:%d", fd_, GetIP(), GetPort(), (int) userCount);
}

/**
 * @brief 关闭一个客户端连接
 */
void HttpConn::Close() {
    //释放 HttpResponse 类对象中的内存映射文件
    response_.UnmapFile();
    //判断连接是否已经关闭
    if (isClose_ == false) {
        //如果连接没用被关闭
        isClose_ = true;    //表示已经关闭连接
        userCount--;        //客户端数量减一
        close(fd_);     //关闭文件描述符
        //日志记录
        LOG_INFO("Client[%d](%s:%d) quit, UserCount:%d", fd_, GetIP(), GetPort(), (int) userCount);
    }
}

/**
 * @brief 获取当前连接的文件描述符
 * @return 当前文件的文件描述符
 */
int HttpConn::GetFd() const {
    return fd_;
};

/**
 * @brief 获取当前连接的客户端地址信息
 * @return
 */
struct sockaddr_in HttpConn::GetAddr() const {
    return addr_;
}

/**
 * @brief 获取连接的IP地址
 * @return
 */
const char *HttpConn::GetIP() const {
    return inet_ntoa(addr_.sin_addr);
}

/**
 * @brief 获取连接的端口号
 * @return
 */
int HttpConn::GetPort() const {
    return addr_.sin_port;
}

/**
 * @brief 从客户端连接中读取数据
 * @param saveErrno 保存读取数据时发生的错误信息
 * @return
 */
ssize_t HttpConn::read(int *saveErrno) {
    ssize_t len = -1;
    //使用循环不断读取数据
    do {
        len = readBuff_.ReadFd(fd_, saveErrno);
        if (len <= 0) {
            break;
        }
    } while (isET); //表示当前是否使用了边缘触发模式进行读取
    return len;
}

/**
 * @brief 向客户端写入数据
 * @param saveErrno 保存写入数据时发生的错误信息
 * @return
 */
ssize_t HttpConn::write(int *saveErrno) {
    ssize_t len = -1;
    do {
        //将保存在iov中的数据写入到文件描述符fd中,并将返回的字节数保存在len中
        len = writev(fd_, iov_, iovCnt_);
        if (len <= 0) {
            *saveErrno = errno;
            break;
        }
        if (iov_[0].iov_len + iov_[1].iov_len == 0) { break; } /* 传输结束 */
        else if (static_cast<size_t>(len) > iov_[0].iov_len) {
            iov_[1].iov_base = (uint8_t *) iov_[1].iov_base + (len - iov_[0].iov_len);
            iov_[1].iov_len -= (len - iov_[0].iov_len);
            if (iov_[0].iov_len) {
                writeBuff_.RetrieveAll();
                iov_[0].iov_len = 0;
            }
        } else {
            iov_[0].iov_base = (uint8_t *) iov_[0].iov_base + len;
            iov_[0].iov_len -= len;
            writeBuff_.Retrieve(len);
        }
    } while (isET || ToWriteBytes() > 10240);   //判断当前待写入数据的大小是否超过了 10KB
    return len;
}

/**
 * HTTP连接处理函数
 * @brief 解析HTTP请求并返回HTTP响应，其中包括响应头和文件内容
 * @return
 */
bool HttpConn::process() {
    //1.初始化HTTP请求对象
    request_.Init();
    //2.检查读缓冲区是否有数据可读
    if (readBuff_.ReadableBytes() <= 0) {
        return false;
    //3.调用 request_.parse(readBuff_) 解析HTTP请求
    } else if (request_.parse(readBuff_)) {
    //4.解析成功,调用response_.Init初始化HTTP响应对象，200表示响应状态码
        LOG_DEBUG("%s", request_.path().c_str());
        //srcDir 是服务器根目录、request_.path() 是请求的文件路径、request_.IsKeepAlive() 表示是否保持长连接
        response_.Init(srcDir, request_.path(), request_.IsKeepAlive(), 200);
    } else {
    //5.解析失败,调用response_.Init初始化HTTP响应对象，400表示响应状态码
        response_.Init(srcDir, request_.path(), false, 400);
    }

    //6.调用 response_.MakeResponse(writeBuff_) 生成HTTP响应消息体
    response_.MakeResponse(writeBuff_);
    //将响应头写入 iov_[0] 中，将响应文件写入 iov_[1] 中
    /* 响应头 */
    iov_[0].iov_base = const_cast<char *>(writeBuff_.Peek());
    iov_[0].iov_len = writeBuff_.ReadableBytes();
    iovCnt_ = 1;

    /* 文件 */
    //根据文件是否存在和大小来决定是否写入 iov_[1] 中
    if (response_.FileLen() > 0 && response_.File()) {
        iov_[1].iov_base = response_.File();
        iov_[1].iov_len = response_.FileLen();
        iovCnt_ = 2;
    }
    //7.大有服务器日志
    LOG_DEBUG("filesize:%d, %d  to %d", response_.FileLen(), iovCnt_, ToWriteBytes());
    return true;
}
