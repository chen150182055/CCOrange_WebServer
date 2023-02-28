#ifndef SQLCONNPOOL_H
#define SQLCONNPOOL_H

#include <mysql/mysql.h>
#include <string>
#include <queue>
#include <mutex>
#include <semaphore.h>
#include <thread>
#include "../log/log.h"

class SqlConnPool {
public:
    static SqlConnPool *Instance();

    MYSQL *GetConn();

    void FreeConn(MYSQL *conn);

    int GetFreeConnCount();

    void Init(const char *host, int port,
              const char *user, const char *pwd,
              const char *dbName, int connSize);

    void ClosePool();

private:
    SqlConnPool();

    ~SqlConnPool();

    int MAX_CONN_;  //连接池中的最大连接数
    int useCount_;  //连接池中已经被使用的连接数
    int freeCount_; //连接池中当前可用的连接数

    std::queue<MYSQL *> connQue_;   //存储可用连接的队列
    std::mutex mtx_;//互斥锁，保证对连接池的操作是线程安全的
    sem_t semId_;   //信号量，用于限制连接池中连接的个数
};


#endif // SQLCONNPOOL_H