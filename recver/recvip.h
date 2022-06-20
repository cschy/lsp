#pragma once
#include "business.h"
#include "utils.h"
#include <map>
#include <unordered_set>
#include <string>
#include <iostream>

//command
#define MODULE_IP "post"

class RecvIp : public Business
{
public:
	virtual void accept_callback(MyData* data) {
		addMyData(data);
	}
	virtual void read_callback(MyData* data) {
		if (data->readBuf.empty()) {//logout
			MyData2Name.erase(data);
			//clear businesss data...
			deleteMyData(data);
			return;
		}
		//������Ĵ��䱻ץ���ˣ��ڿ�һֱ��������ᵼ�»�������ը
		MyData* datas = read_parse(data);
		data->readBuf.clear();
		//sendData(datas);
	}

protected:
	virtual void addMyData(MyData* data) {
		MyData2Name[data] = std::string(inet_ntoa(data->userAddr.sin_addr)) + ":" + std::to_string(data->userAddr.sin_port);
	}
	virtual void deleteMyData(MyData* data) { delete data; };
	MyData* read_parse(MyData* data) {
		/*pson request(data->readBuf.c_str());
		if (request[KEY_MODULE] == MODULE_IP)
		{
			std::string recvStr = request["ip"];
			if (!recvStr.empty()) {
				std::cout << "�յ�IP��" << recvStr << std::endl;
				if (ipSet.insert(recvStr).second) {
					std::cout << "�ɹ�����IP��" << recvStr << std::endl;
				}
				data->sendBuf = "�ɹ�����ip";
			}
			else {
				data->sendBuf = "ip�ֶβ���Ϊ��";
			}
			return data;
		}
		data->sendBuf = "unknown module: " + request[KEY_MODULE];
		return data;*/

		std::cout << "recvIP: " << data->readBuf << std::endl;
		return data;
	}

	static std::map<MyData*, std::string> MyData2Name;
	static std::unordered_set<std::string> ipSet;
};

std::map<MyData*, std::string> RecvIp::MyData2Name;
std::unordered_set<std::string> RecvIp::ipSet;