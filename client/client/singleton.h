#ifndef SINGLETON_H
#define SINGLETON_H
#include <memory>
#include <mutex>
#include <iostream>
using namespace std;

// 模板单例基类：
// 1. 保证派生类全局只有一个实例
// 2. 第一次用到时才创建对象
// 3. 线程安全
template<typename T>
class Singleton {
protected:
    // 允许子类构造，但不允许外部直接创建 Singleton
    Singleton() = default;
    // 禁止拷贝，避免复制出第二个实例
    Singleton(const Singleton<T>&) = delete;
    // 禁止赋值，避免破坏单例唯一性
    Singleton&operator=(const Singleton<T>&st) = delete;
    // 保存唯一实例
    static std::shared_ptr<T> _instance;
public:
    // 获取唯一实例的入口
    static std::shared_ptr<T> GetInstance() {
        // call_once 保证这段初始化逻辑只执行一次，且线程安全
        static std::once_flag s_flag;
        std::call_once(s_flag, [&]() {
            // 这里不用 make_shared 的原因：
            // 很多单例子类会把构造函数设成 private，
            // 直接 new T 更适合这种“由基类控制创建”的写法
            _instance = std::shared_ptr<T>(new T);
        });
        // 第一次调用时完成懒加载，后续直接返回同一个对象
        return _instance;
    }
    void PrintAddress() {
        std::cout<<_instance.get()<<std::endl;
    }    
    // 演示用析构函数：实际项目里通常不依赖单例析构顺序
    ~Singleton() {
        std::cout<<"this is the destructor of Singleton"<<std::endl;
    }
};
template<typename T>
std::shared_ptr<T> Singleton<T>::_instance = nullptr;
#endif // SINGLETON_H
