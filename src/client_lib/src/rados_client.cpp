
// C++ std lib
#include <cstdlib>
#include <iostream>
#include <map>
#include <fstream>
#include <exception>
#include <algorithm>
#include <cctype>
#include <cassert>
#include <memory>
#include <utility>



// Project lib
#include "cluster/Worker.h"
#include "cluster/WorkerPool.h"
#include "cluster/CephContext.h"
#include "ipc/IPCServer.h"
#include "remote/ChannelContext.h"
#include "remote/Security.h"
#include "remote/ServerChannel.h"


#ifdef SING_ENABLE_LOG
//Google log
#include <glog/logging.h>
#endif

// number of configuration files
#define NUM_OF_FILES 2
#define KEY_VALUE_SPLIT '='

using namespace singaistorageipc;



void printUsage(const char* programName)
{

  std::cout << "Usage: " << programName << " service_config_file "
            << "cephC_config_file" << std::endl;

}


void enableLogging(const char* programName)
{

// log only if explicitly specialiazed
#ifdef SING_ENABLE_LOG
  // use Google logging for the project.
 FLAGS_stderrthreshold = google::FATAL;
 google::InitGoogleLogging(programName);
 std::cout << "\nLogging IS enabled.\n" <<  std::endl;

#else
  std::cout << "\nLogging IS NOT enabled.\n" << std::endl;
#endif


}


int readLocalConf(std::map<std::string, std::string>& values,
                  const char* fileTitle)
{

  std::ifstream conf(fileTitle, std::ios_base::in);

  if(!conf.is_open())
  {
    try
    {
      conf.open(fileTitle, std::ios_base::in);
    } catch(const std::exception& exp)
    {
      std::cout << "Cannot open file '" << fileTitle << "'\n" 
                << std::endl;
    }
  }

  if(!conf.is_open())
  {
    return 0; // error
  }


  std::string line;
 

  while(!conf.eof())
  {
    std::getline(conf, line, '\n');

    line.erase(std::remove_if(line.begin(), line.end(), isspace), line.end());
    
    auto splitIter = line.find(KEY_VALUE_SPLIT, 0);

    if(splitIter == std::string::npos)
    {
      continue; // wrong configuration line
    }

    // found
    std::string key(std::move(line.substr(0, splitIter)));
    std::string value(std::move(line.substr(splitIter+1, std::string::npos)));

    // non-empty length
    assert(key.length() && value.length());

    // key-value pair 
    values.emplace(key, value);
  }// while
  

  assert(!values.empty());

  // close the file
  conf.close(); 

  return 1; // success
}


int readConfigFiles(std::map<std::string, std::string>& values,
                    const char** confFiles, const int numOfFiles)
{
  // the first file is the configuration file
  // used by internal modules to initialize them

  auto readRes = readLocalConf(values, confFiles[0]);
  if (!readRes)
  {
    return readRes;
  }



  return readRes; // success

  // the last file is Ceph configuration file
  // it's not read here but is used later.

}


void printError(const char* errMsg)
{
  std::cout << errMsg << std::endl;
  std::exit(EXIT_SUCCESS);
}


int main(const int argc, const char* argv[])
{

  if(argc != NUM_OF_FILES + 1)
  {
    // must pass configuration files
    printUsage(argv[0]);
	return EXIT_SUCCESS;
  }

  enableLogging(argv[0]);

  std::map<std::string, std::string>  configMap;

  auto readRes = readConfigFiles(configMap, (argv+1), argc-1);
  
  if(!readRes)
  {
    return EXIT_SUCCESS;
  }

  
  decltype(configMap.begin()) confIter;


  // initializing the remote part of the library
  confIter = configMap.find("auth_server_ip");
  if(confIter == configMap.end())
  {
    printError("Cannot find 'auth_server_address'");
  }
  
  std::string auth_ip(confIter->second);
  
  confIter = configMap.find("auth_server_port");
  if(confIter == configMap.end())
  {
    printError("Cannot find 'auth_server_port'");
  }

  const unsigned short portNo = static_cast<const unsigned short> (std::stoi(confIter->second));


  ChannelContext chContext;
  chContext.remoteServerAddress_ = std::move(auth_ip);
  chContext.port_                = portNo;


  std::unique_ptr<ServerChannel> channel = std::make_unique<ServerChannel>(chContext);

  assert(channel->initChannel());
  
  auto securityPtr = Security::createSharedSecurityModule("Default", 
                                          std::move(channel));

  assert(securityPtr);

  assert(securityPtr->initialize());

  // start the security service
  securityPtr->startService();
  

  auto workerPool = WorkerPool::createWorkerPool("Default", 0, 0);

  // creating and starting the IPC components 
    

  return EXIT_SUCCESS;
}
