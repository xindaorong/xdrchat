#pragma once
#include<memory>
#include<mutex>
#include<iostream>
template <typename T>
class Singleton
{
protected:
	Singleton() = default;
	Singleton(const Singleton<T>&) = delete;
	Singleton& operator=(const Singleton<T>& st) = delete;

	static std::shared_ptr<T>_instance;
public:
	static std::shared_ptr<T>GetInstance()
	{
		static std::once_flag s_flag;
		std::call_once(s_flag, [&]()
			{
				_instance = std::shared_ptr<T>(new T);
			});
		return _instance;//第二次执行lambda表达式的时候就直接返回 _instance了
	}
	
	void PrintAddress()
	{
		std::cout << _instance.get() << std::endl;
	}
	~Singleton()
	{
		std::cout << "this singleton is construcked"<<std::endl;
	}
	
};
template <typename T>
std::shared_ptr<T> Singleton<T>::_instance = nullptr;