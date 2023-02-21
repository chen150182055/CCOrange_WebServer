#ifndef BUFFER_H
#define BUFFER_H

#include <cstring>      //perror
#include <iostream>
#include <unistd.h>     // write
#include <sys/uio.h>    //readv
#include <vector>       //readv
#include <atomic>
#include <assert.h>

//用于缓冲数据的类
class Buffer {
public:
    Buffer(int initBuffSize = 1024);

    ~Buffer() = default;

    size_t WritableBytes() const;

    size_t ReadableBytes() const;

    size_t PrependableBytes() const;

    const char *Peek() const;

    void EnsureWriteable(size_t len);

    void HasWritten(size_t len);

    void Retrieve(size_t len);

    void RetrieveUntil(const char *end);

    void RetrieveAll();

    std::string RetrieveAllToStr();

    const char *BeginWriteConst() const;

    char *BeginWrite();

    void Append(const std::string &str);

    void Append(const char *str, size_t len);

    void Append(const void *data, size_t len);

    void Append(const Buffer &buff);

    ssize_t ReadFd(int fd, int *Errno);

    ssize_t WriteFd(int fd, int *Errno);

private:
    //这两个方法一个是常量成员函数，一个是非常量成员函数，因此分别用于常量对象和非常量对象调用
    char *BeginPtr_();              //返回缓存的起始位置（写入位置）的指针

    const char *BeginPtr_() const;  //返回缓存的起始位置（写入位置）的指针

    void MakeSpace_(size_t len);    //为缓存中的数据腾出足够的空间，以便在缓存中写入更多的数据

    std::vector<char> buffer_;          //存储缓存中的数据
    std::atomic <std::size_t> readPos_; //记录缓存中已读的数据的位置
    std::atomic <std::size_t> writePos_;//记录缓存中已写的数据的位置
};

#endif //BUFFER_H