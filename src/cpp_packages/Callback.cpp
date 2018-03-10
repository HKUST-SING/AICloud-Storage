#pragma once

#include <iostream>




namespace singaistorageipc{

void writeSuccess() override{
    	std::cout << fd_ << ":write successfully" << std::endl
};

void ServerWriteCallback::writeErr(size_t bytesWritten, 
    	const AsyncSocketException& ex) override{
    	std::cout << fd_ << ":" << ex.what() << std::endl;
};

}