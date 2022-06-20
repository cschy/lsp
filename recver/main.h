#pragma once

#include <arpa/inet.h>
#include <string>

struct MyData //like channel?
{
    int fd;
    sockaddr_in userAddr;
    std::string readBuf;
    std::string sendBuf;
};


MyData* recvData(MyData* data);

void sendData(MyData* data);