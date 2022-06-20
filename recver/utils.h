#pragma once
#define _CRT_SECURE_NO_WARNINGS


#include <string.h>
#include <map>
#include <string>

void errif(bool condition, const char* errmsg) {
	if (condition) {
		perror(errmsg);
		exit(EXIT_FAILURE);
	}
}

class pson
{
public:
	pson() = default;
	pson(const char* str) : strData(str) {
		decode();
	}

	const std::string& toString() const
	{
		return strData;
	}

#define KEY_MODULE "module"
#define KEY_END "end"
	std::string& operator[](const char* key) {
		if (strcmp(key, KEY_MODULE) == 0) {
			return strModule;
		}
		if (strcmp(key, KEY_END) == 0) {
			encode();
			return strModule;
		}

		return mapData[key];
	}

	bool find(const char* key) {
		return (mapData.find(key) != mapData.end());
	}

	void encode()
	{
		strData += (strModule + '?');
		for (auto& i : mapData)
		{
			strData += (std::string(i.first) + ':' + i.second + ',');
		}
	}
	void decode()
	{
		//模块
		std::size_t posModuleTail = strData.find_first_of('?');
		strModule = strData.substr(0, posModuleTail);
		if (posModuleTail == std::string::npos) {
			return;
		}

		std::string data = strData.substr(posModuleTail + 1);
#ifdef __GNUC__
		char tmp[data.length() + 1];
		tmp[data.length()] = '\0';
		memcpy(tmp, data.c_str(), data.length());
		char* token = strtok(tmp, ",");
		
		while (token != NULL) {
			char* p;
			for (p = token; (*p != ':') && (*p != '\0'); p++);
			if ((*p == ':') && (*(p + 1) != '\0')) {
				char* value = p + 1;
				*p = '\0';
				mapData[token] = value;
			}
			if (data == token) {//没有分隔符
				break;
			}
			token = strtok(NULL, ",");
		}
#elif _MSC_VER
		char* tmp = new char[data.length() + 1];
		tmp[data.length()] = '\0';
		memcpy(tmp, data.c_str(), data.length());
		char* token = strtok(tmp, ",");

		while (token != NULL) {
			char* p;
			for (p = token; (*p != ':') && (*p != '\0'); p++);
			if ((*p == ':') && (*(p + 1) != '\0')) {
				char* value = p + 1;
				*p = '\0';
				mapData[token] = value;
			}
			if (data == token) {//没有分隔符
				break;
			}
			token = strtok(NULL, ",");
		}
		delete []tmp;
#endif
		
	}

private:
	std::string strModule;
	std::map<std::string, std::string> mapData;
	std::string strData;
};