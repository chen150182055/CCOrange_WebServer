#ifndef LOG_H
#define LOG_H

#include <mutex>
#include <string>
#include <thread>
#include <sys/time.h>
#include <string.h>
#include <stdarg.h>           // vastart va_end
#include <assert.h>
#include <sys/stat.h>         //mkdir
#include "blockqueue.h"
#include "../buffer/buffer.h"

class Log {
public:
    void init(int level, const char *path = "./log",
              const char *suffix = ".log",
              int maxQueueCapacity = 1024);

    static Log *Instance();

    static void FlushLogThread();

    void write(int level, const char *format, ...);

    void flush();

    int GetLevel();

    void SetLevel(int level);

    bool IsOpen() { return isOpen_; }

private:
    Log();

    void AppendLogLevelTitle_(int level);

    virtual ~Log();

    void AsyncWrite_();

private:
    static const int LOG_PATH_LEN = 256;    //日志文件路径的最大长度为256
    static const int LOG_NAME_LEN = 256;    //日志文件名的最大长度为256
    static const int MAX_LINES = 50000;     //日志缓冲区中最多保存的日志行数为50000

    const char *path_;      //日志文件路径的指针
    const char *suffix_;    //日志文件名后缀的指针

    int MAX_LINES_;         //日志缓冲区中最多保存的日志行数

    int lineCount_;         //已经写入日志文件的日志行数
    int toDay_;             //当前的日期，用于判断是否需要创建新的日志文件

    bool isOpen_;           //日志系统是否打开

    Buffer buff_;           //日志缓冲区，用于存储待写入日志文件的内容
    int level_;             //日志级别
    bool isAsync_;          //是否启用异步写日志

    FILE *fp_;              //日志文件指针，用于向文件中写入日志内容
    std::unique_ptr <BlockDeque<std::string>> deque_;   //用于在异步写日志时实现线程安全的阻塞队列
    std::unique_ptr <std::thread> writeThread_;         //用于异步写日志的线程指针
    std::mutex mtx_;        //用于线程安全的互斥锁
};

//宏LOG_BASE，用于记录日志
//宏接受三个参数，第一个是日志的级别，第二个是日志的格式，第三个是可变参数列表
#define LOG_BASE(level, format, ...) \
    do {\
        Log* log = Log::Instance();\
        if (log->IsOpen() && log->GetLevel() <= level) {\
            log->write(level, format, ##__VA_ARGS__); \
            log->flush();\
        }\
    } while(0);

#define LOG_DEBUG(format, ...) do {LOG_BASE(0, format, ##__VA_ARGS__)} while(0);
#define LOG_INFO(format, ...) do {LOG_BASE(1, format, ##__VA_ARGS__)} while(0);
#define LOG_WARN(format, ...) do {LOG_BASE(2, format, ##__VA_ARGS__)} while(0);
#define LOG_ERROR(format, ...) do {LOG_BASE(3, format, ##__VA_ARGS__)} while(0);

#endif //LOG_H