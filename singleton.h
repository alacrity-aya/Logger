#pragma once

#include <iostream>
#include <memory>
#include <mutex>
template <typename T>
class Singleton {
protected:
    Singleton() = default;
    Singleton(const Singleton<T>&) = delete;
    Singleton& operator=(const Singleton<T>& st) = delete;
    static std::shared_ptr<T> _instance;

public:
    [[nodiscard]] static std::shared_ptr<T> get_instance()
    {
        static std::once_flag s_flag;
        std::call_once(s_flag, [&]() {
            _instance = std::shared_ptr<T>(new T);
        });
        return _instance;
    }
    void print_address()
    {
        std::cout << _instance.get() << std::endl;
    }
    ~Singleton()
    {
        std::cout << "this is singleton destruct" << std::endl;
    }
};
template <typename T>
std::shared_ptr<T> Singleton<T>::_instance = nullptr;
