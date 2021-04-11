#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <mutex>                //互斥锁
#include <condition_variable>   //条件变量
#include <queue>                //实现任务队列
#include <thread>
#include <functional>
class ThreadPool {
public:
    explicit ThreadPool(size_t threadCount = 8): pool_(std::make_shared<Pool>()) {
            assert(threadCount > 0);

            // 创建threadCount个子线程
            for(size_t i = 0; i < threadCount; i++) {
                std::thread([pool = pool_] {//线程要执行的代码
                    std::unique_lock<std::mutex> locker(pool->mtx);//锁
                    while(true) {//死循环，不断从队列中取数据
                        if(!pool->tasks.empty()) {
                            // 从任务队列中取第一个任务
                            auto task = std::move(pool->tasks.front());
                            // 移除掉队列中第一个元素
                            pool->tasks.pop();
                            locker.unlock();
                            task();
                            locker.lock();
                        } 
                        else if(pool->isClosed) break;
                        else pool->cond.wait(locker);   // 如果队列为空，线程阻塞等待
                    }
                }).detach();// 线程分离
            }
    }

    ThreadPool() = default;

    ThreadPool(ThreadPool&&) = default;
    
    ~ThreadPool() {
        if(static_cast<bool>(pool_)) {
            {
                std::lock_guard<std::mutex> locker(pool_->mtx);
                pool_->isClosed = true;//线程关闭置为true，最终while循环时，会执行break，一个个跳出循环，再执行线程分离
            }
            pool_->cond.notify_all();//把所有线程唤醒
        }
    }

    template<class F>
    void AddTask(F&& task) {//往队列里添加任务 
        {
            std::lock_guard<std::mutex> locker(pool_->mtx);
            pool_->tasks.emplace(std::forward<F>(task));//添加任务队列
        }
        pool_->cond.notify_one();   // 唤醒一个等待的线程
    }

private:
    // 结构体
    struct Pool {
        std::mutex mtx;     // 互斥锁
        std::condition_variable cond;   // 条件变量
        bool isClosed;          // 是否关闭
        std::queue<std::function<void()>> tasks;    // 队列（保存的是任务）
    };
    std::shared_ptr<Pool> pool_;  //  池子
};


#endif //THREADPOOL_H