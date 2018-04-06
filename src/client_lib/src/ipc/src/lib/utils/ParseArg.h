/*
 * Parse arguments at the start of macheine.
 */
#pragma once

#include <map>

namespace singaistorageipc{

class Parser{
public:
	Parser(int argc,char** argv):
		argc(argc),
		argv(argv){};

	std::map<char,int> parse(int argc,char** argv){
		std::map<char,int> a;
		return a;
	};

private:
	int argc;
	char** argv;
};


}