#pragma once
///这个头文件是一个“INI 配置分组容器”，让配置能够这样读取：
//cfg["SelfServer"]["Port"];
#include<map>
#include<iostream>
#include<string>
struct SectionInfo
{
	SectionInfo() = default;
	~SectionInfo() = default;
	SectionInfo(const SectionInfo& src) = default;
	//分别重载=和[]这俩个操作符
	SectionInfo& operator=(const SectionInfo& src)
	{
		if (this != &src)
		{
			_section_datas = src._section_datas;
		}
		return *this;
	}
	std::string operator[](const std::string& key)
	{
		if (_section_datas.find(key) == _section_datas.end())
		{
			return "";
		}
		return _section_datas[key];
	}
private:
	std::map<std::string, std::string>_section_datas;
};