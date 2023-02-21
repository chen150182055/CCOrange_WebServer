#ifndef BLOCKQUEUE_H
#define BLOCKQUEUE_H

#include <mutex>
#include <deque>
#include <condition_variable>
#include <sys/time.h>

//基于std::deque的类模板,提供了许多用于在线程之间进行数据传输的函数
//它使用标准库mutex、condition_variable和sys/time.h来实现互斥和同步
template<class T>
class BlockDeque {
public:
    explicit BlockDeque(size_t MaxCapacity = 1000);

    ~BlockDeque();

    void clear();

    bool empty();

    bool full();

    void Close();

    size_t size();

    size_t capacity();

    T front();

    T back();

    void push_back(const T &item);

    void push_front(const T &item);

    bool pop(T &item);

    bool pop(T &item, int timeout);

    void flush();

private:
    std::deque <T> deq_;    //一个 deque，用于存储元素

    size_t capacity_;       //队列的最大容量

    std::mutex mtx_;        //一个互斥锁，用于保护共享资源的访问

    bool isClose_;          //一个标志，表示队列是否关闭

    //条件变量的作用是在等待某个条件变为 true 时暂停线程的执行，以避免忙等待
    std::condition_variable condConsumer_;  //条件变量，用于在消费者等待数据时进行同步

    std::condition_variable condProducer_;  //条件变量，用于在生产者等待空间时进行同步
};

/**
 * @brief 构造函数,用于创建一个 BlockDeque 实例并初始化其成员变量
 * @tparam T 表示 BlockDeque 模板类的类型
 * @param MaxCapacity
 */
template<class T>
BlockDeque<T>::BlockDeque(size_t MaxCapacity) :capacity_(MaxCapacity) {
    assert(MaxCapacity > 0);    //检查传入的最大容量参数是否大于 0
    isClose_ = false;
}

/**
 * @brief 析构函数
 * 因为这个类使用了 C++11 中的 RAII 技术，在对象被销毁时，它所占用的资源（如互斥锁和条件变量）会被自动释放
 * @tparam T
 */
template<class T>
BlockDeque<T>::~BlockDeque() {
    //清空队列并关闭队列
    Close();
};

/**
 * @brief 关闭阻塞队列
 * @tparam T
 */
template<class T>
void BlockDeque<T>::Close() {
    {
        //使用了std::lock_guard来对互斥量进行加锁，以保证多线程环境下只有一个线程可以访问容器的数据，避免竞争和冲突
        std::lock_guard <std::mutex> locker(mtx_);
        //清空阻塞队列 deq_
        deq_.clear();
        //设置 isClose_ 为 true，表示队列已经关闭
        isClose_ = true;
    }
    //唤醒所有等待的线程,包括消费者线程和生产者线程，使它们都能够退出阻塞状态
    condProducer_.notify_all();
    condConsumer_.notify_all();
};

/**
 * @brief 通知一个消费者线程，让它从阻塞状态中唤醒并尝试取走队列中的元素，以便退出
 * @tparam T
 */
template<class T>
void BlockDeque<T>::flush() {
    condConsumer_.notify_one();
};

/**
 * @brief 清空容器中所有元素的功能
 * @tparam T
 */
template<class T>
void BlockDeque<T>::clear() {
    //使用了std::lock_guard来对互斥量进行加锁，以保证多线程环境下只有一个线程可以访问容器的数据，避免竞争和冲突
    std::lock_guard <std::mutex> locker(mtx_);
    //清空容器中所有元素
    deq_.clear();
}

/**
 * @brief 返回队列的第一个元素
 * @tparam T
 * @return
 */
template<class T>
T BlockDeque<T>::front() {
    //使用了std::lock_guard来对互斥量进行加锁，以保证多线程环境下只有一个线程可以访问容器的数据，避免竞争和冲突
    std::lock_guard <std::mutex> locker(mtx_);
    //返回队列 deq_ 中的第一个元素，即头部元素
    return deq_.front();
}

/**
 * @brief 返回双端队列的尾部元素
 * @tparam T
 * @return
 */
template<class T>
T BlockDeque<T>::back() {
    //使用了std::lock_guard来对互斥量进行加锁，以保证多线程环境下只有一个线程可以访问容器的数据，避免竞争和冲突
    std::lock_guard <std::mutex> locker(mtx_);
    return deq_.back();
}

/**
 * @brief 获取当前阻塞队列中元素的个数
 * @tparam T
 * @return
 */
template<class T>
size_t BlockDeque<T>::size() {
    //使用了std::lock_guard来对互斥量进行加锁，以保证多线程环境下只有一个线程可以访问容器的数据，避免竞争和冲突
    std::lock_guard <std::mutex> locker(mtx_);
    return deq_.size();
}

/**
 * @brief 返回容器的最大容量
 * @tparam T
 * @return
 */
template<class T>
size_t BlockDeque<T>::capacity() {
    //使用了std::lock_guard来对互斥量进行加锁，以保证多线程环境下只有一个线程可以访问容器的数据，避免竞争和冲突
    std::lock_guard <std::mutex> locker(mtx_);
    return capacity_;
}

/**
 * @brief 向BlockDeque容器中添加元素
 * @tparam T
 * @param item
 */
template<class T>
void BlockDeque<T>::push_back(const T &item) {
    //通过std::mutex锁定对象
    std::unique_lock <std::mutex> locker(mtx_);
    //检查当前容器的大小是否超过预定的容量
    while (deq_.size() >= capacity_) {  //超过
        //等待条件condProducer_满足
        condProducer_.wait(locker);
    }
    //如果没有超过，则将新的元素添加到容器中，最后唤醒条件condConsumer_。
    deq_.push_back(item);
    condConsumer_.notify_one();
}

/**
 * @brief 在队列的前面插入元素
 * @tparam T
 * @param item
 */
template<class T>
void BlockDeque<T>::push_front(const T &item) {
    //首先使用unique_lock对互斥锁进行加锁
    std::unique_lock <std::mutex> locker(mtx_);
    //使用while循环判断队列是否已满
    while (deq_.size() >= capacity_) {
        //等待condProducer_通知
        condProducer_.wait(locker);
    }
    //将元素插入到队列头部
    deq_.push_front(item);
    //通过condConsumer_通知其他线程队列已有元素
    condConsumer_.notify_one();
}

/**
 * @brief 返回当前双端队列中是否有元素
 * @tparam T
 * @return
 */
template<class T>
bool BlockDeque<T>::empty() {
    //使用了 std::lock_guard 对 mtx_ 加锁，以避免并发访问时出现冲突
    std::lock_guard <std::mutex> locker(mtx_);
    return deq_.empty();
}

/**
 * @brief 判断双端队列 deq_ 是否已经满
 * @tparam T
 * @return
 */
template<class T>
bool BlockDeque<T>::full() {
    //使用 std::lock_guard 对 mtx_ 进行加锁，以确保线程安全
    std::lock_guard <std::mutex> locker(mtx_);
    return deq_.size() >= capacity_;
}

/**
 * @brief 从 BlockDeque 队列中取出一个元素
 * @tparam T
 * @param item
 * @return
 */
template<class T>
bool BlockDeque<T>::pop(T &item) {
    //使用了 std::unique_lock 对象锁定 mtx_，避免多个线程同时访问队列时发生冲突
    std::unique_lock <std::mutex> locker(mtx_);
    //队列为空时，该函数将一直等待，直到队列中有元素被插入
    while (deq_.empty()) {
        condConsumer_.wait(locker);
        if (isClose_) {
            return false;
        }
    }
    //当队列中有元素时，它将队列的第一个元素取出来，并将其弹出
    item = deq_.front();
    deq_.pop_front();
    //调用 condProducer_.notify_one() 来通知其他等待的线程队列中的元素数量已经发生了变化
    condProducer_.notify_one();
    return true;
}

/**
 * @brief 阻塞队列的弹出
 * @tparam T
 * @param item 用于保存从队首弹出的元素值
 * @param timeout 队列为空，则在超时时间内等待直到队列不为空
 * @return
 */
template<class T>
bool BlockDeque<T>::pop(T &item, int timeout) {
    //使用了std::unique_lock实现互斥锁
    std::unique_lock <std::mutex> locker(mtx_);
    //如果队列为空，则在超时时间内等待
    while (deq_.empty()) {
        //在超时时间内等待
        if (condConsumer_.wait_for(locker, std::chrono::seconds(timeout))
            == std::cv_status::timeout) {
            return false;
        }
        if (isClose_) {
            return false;
        }
    }
    item = deq_.front();
    deq_.pop_front();
    condProducer_.notify_one();
    return true;
}

#endif // BLOCKQUEUE_H