/*
 * Parse arguments at the start of macheine.
 */
#pragma once;

#include <map>

namespace singaistorageipc{

class Parser{
public:
	Parser(int argc,char** argv):
		argc(argc),
		argv(argv){};

	map<char,int> parse(int argc,char** argv){
		return nullptr;
	};

private:
	int argc;
	char** argv;
};


}