#pragma once

struct MyData;

class Business
{
public:
	virtual ~Business() {}
	
public:
	virtual void accept_callback(MyData* data) { printf("Business::accept_callback()\n"); };
	virtual void read_callback(MyData* data) { printf("Business::read_callback()\n"); };

protected:
	virtual void addMyData(MyData* data) { printf("Business::addMyData()\n"); };
	virtual void deleteMyData(MyData* data) { printf("Business::deleteMyData()\n"); };
};