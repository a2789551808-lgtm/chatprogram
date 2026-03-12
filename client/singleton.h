#ifndef SINGLETON_H
#define SINGLETON_H

/******************************************************************************
 *
 * @file       singleton.h
 * @brief      单例模板类
 *
 * @author     君安
 * @date       2026/03/02
 * @history
 *****************************************************************************/

#include "global.h"

template <typename T>
class Singleton {
protected:
    Singleton() = default;
    Singleton(const Singleton<T>&) = delete;
    Singleton& operator= (const Singleton<T>&) = delete;

    static std::shared_ptr<T> _instance;
public:
    static std::shared_ptr<T> GetInstance() {
        static std::once_flag s_flag;
        std::call_once(s_flag, []() {
            _instance = std::shared_ptr<T>(new T);    //在c++11中，可以直接用局部静态变量，不需要这样复杂的加锁和使用call_once
        });

        return _instance;
    }

    void PrintAddress() {
        std::cout << _instance.get() << std::endl;
    }

    ~Singleton() {
        std::cout << "this is singleton destruct" << std::endl;
    }
};

template <typename T>
std::shared_ptr<T> Singleton<T>::_instance = nullptr;   //静态成员要在类外定义，或者在c++17可以用inline内联

#endif // SINGLETON_H
