#pragma once
#include<map>
#include<iostream>
#include<string>
//此类用来管理key和value
struct SectionInfo
{
	SectionInfo() = default;
	~SectionInfo() = default;
	SectionInfo(const SectionInfo& src) = default;
	SectionInfo& operator=(const SectionInfo& src) {
		if (this != &src)
		{
			_section_datas = src._section_datas;
		}
		return *this;
	}
	std::map<std::string, std::string>_section_datas;
	std::string operator[](const std::string& key) {
		if (_section_datas.find(key) == _section_datas.end()) {
			return"";
		}
		return _section_datas[key];
	}
};
