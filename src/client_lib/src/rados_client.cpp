
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
#include <sstream>
#include <cctype>
#include <thread>
#include <csignal>
#include <cerrno>
#include <cstring>
#include <unistd.h>

// Facebook folly
#include <folly/io/async/EventBase.h>

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
#define EMPTY_FILEPATH ""

using namespace singaistorageipc;


/*Signal handler to terminate the program in a nice way.*/
static int signal_fd[2] = {0, 0}; 

void signal_shutdown()
{
  int val = 0;
  void* buf = &val;
  size_t count = sizeof(val);
  size_t cnt = 0;
  ssize_t r;
  while(cnt < count)
  {
    r = write(signal_fd[0],buf,count - cnt);
    if(r < 0)
    {
	if(errno == EINTR)
	{
	  continue;
	}
	r = -errno;
	break;
    }
    cnt += r;
    buf = (char*)buf + r;
  }

  if(r < 0)
  {
    #ifdef SING_ENABLE_LOG
    LOG(WARNING) << "ERROR: " << __func__ << ": write() returned" << std::strerror(errno);
    #endif
    exit(0);
  }
}

static void wait_shutdown()
{
  int val;
  void* buf = &val;
  size_t count = sizeof(val);
  size_t cnt = 0;
  ssize_t r;
  while(cnt < count)
  {
    r = read(signal_fd[1],buf,count - cnt);
    if(r <= 0)
    {
	if(r == 0)
	{
	  break;
	}
	if(errno == EINTR)
	{
	  continue;
	}
	r = -errno;
	break;
    }
    cnt += r;
    buf = (char*)buf + r;
  }

  if(r < 0)
  { 
    #ifdef SING_ENABLE_LOG
    LOG(WARNING) << "read signal reutrn with error";
    #endif
  }
}

static int signal_fd_init()
{
  return socketpair(AF_UNIX,SOCK_STREAM,0,signal_fd);
}

static void signal_fd_finalize()
{
  close(signal_fd[0]);
  close(signal_fd[1]);
}

static void handle_sigterm(int signum)
{
  #ifdef SING_ENABLE_LOG
  LOG(INFO) << "signum: " << signum;
  #endif
 
  signal_shutdown();
}

void printUsage(const char* programName)
{

  std::cout << "Usage: " << programName << " service_config_file "
            << "cephC_config_file" << std::endl;

}


void enableLogging(const char* programName, 
                   const char* infoLogFile,
                   const char* warnLogFile,
                   const char* errorLogFile,
                   const char* fatalLogFile)
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

    line.erase(std::remove_if(std::begin(line), std::end(line), isspace), std::end(line));
    std::transform(std::begin(line), std::end(line), std::begin(line),
                   [](unsigned char citem) { return std::tolower(citem); });
    
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

void updateIPCContext(IPCContext &ctx, const std::map<std::string,std::string> &vals)
{
	std::stringstream keyStr;

         keyStr << "ipc_backlog";
         
         auto ipcIter = vals.find(keyStr.str());
         if(ipcIter != vals.end())
         {
           ctx.backlog_ = std::stoi(ipcIter->second);
         }
         
         keyStr.clear();
         keyStr.str(std::string());
        
         keyStr << "ipc_buffsersize";

         ipcIter = vals.find(keyStr.str());

         if(ipcIter != vals.end())
         {
           keyStr.clear();
           keyStr.str(ipcIter->second);
           keyStr >> ctx.bufferSize_;
         }         
         
         keyStr.clear();
         keyStr.str(std::string());

         keyStr << "ipc_minallocbuf";
         
         ipcIter = vals.find(keyStr.str());

         if(ipcIter != vals.end())
         {
           keyStr.clear();
           keyStr.str(ipcIter->second);
           keyStr >> ctx.minAllocBuf_;
         }

         keyStr.clear();
         keyStr.str(std::string());

         keyStr << "ipc_newallocsize";
         
         ipcIter = vals.find(keyStr.str());

         if(ipcIter != vals.end())
         {
           keyStr.clear();
           keyStr.str(ipcIter->second);
           keyStr >> ctx.newAllocSize_;
         }

         keyStr.clear();
         keyStr.str(std::string());
         
         keyStr << "ipc_readsmsize";
         
         ipcIter = vals.find(keyStr.str());

         if(ipcIter != vals.end())
         {
           keyStr.clear();
           keyStr.str(ipcIter->second);
           keyStr >> ctx.readSMSize_;
         }

         keyStr.clear();
         keyStr.str(std::string());

         keyStr << "ipc_writesmsize";
         
         ipcIter = vals.find(keyStr.str());

         if(ipcIter != vals.end())
         {
           keyStr.clear();
           keyStr.str(ipcIter->second);
           keyStr >>  ctx.writeSMSize_;
         }

}

int main(const int argc, const char* argv[])
{

  if(argc != NUM_OF_FILES + 1)
  {
    // must pass configuration files
    printUsage(argv[0]);
	return EXIT_SUCCESS;
  }

  // create the socket pair for signal handler
  if(signal_fd_init() < 0)
  {
    printError(std::strerror(errno));
  }


  std::map<std::string, std::string>  configMap;

  auto readRes = readConfigFiles(configMap, (argv+1), argc-1);
  
  if(!readRes)
  {
    printError("Cannot read configuration files.");
  }

  decltype(configMap.begin()) confIter;
  
  confIter = configMap.find(std::string("info_log_file"));
  const char* infoLog = (confIter != configMap.end()) ? confIter->second.c_str() : nullptr;
  
  confIter = configMap.find(std::string("warning_log_file"));
  const char* warnLog = (confIter != configMap.end()) ? confIter->second.c_str()  : nullptr;

  confIter = configMap.find(std::string("error_log_file"));
  const char* errLog = (confIter != configMap.end()) ? confIter->second.c_str()   : nullptr;

  confIter = configMap.find(std::string("fatal_log_file"));
  const char* fatalLog = (confIter != configMap.end()) ? confIter->second.c_str() : nullptr;


  enableLogging(argv[0], infoLog, warnLog, errLog, fatalLog);

  // reset all filenames
  infoLog  = nullptr;
  warnLog  = nullptr;
  errLog   = nullptr;
  fatalLog = nullptr;
  

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
  confIter = configMap.find("ipc_socket");
  if(confIter == configMap.end())
  {
    printError("Cannot find 'ipc_socket'");
  }
  IPCContext ipccxt(confIter->second,securityPtr,workerPool);
  updateIPCContext(ipccxt,configMap);  
 
  folly::EventBase evb;

  IPCServer ipcserver(ipccxt,&evb);
  std::thread ipcserverthread(&IPCServer::start, &ipcserver);


  configMap.clear(); // release the map resources


  std::signal(SIGTERM,handle_sigterm); 
  std::signal(SIGINT ,handle_sigterm);
  std::signal(SIGSEGV,handle_sigterm);

  wait_shutdown();

  evb.terminateLoopSoon();
  ipcserverthread.join();
  ipcserver.stop();  
  workerPool->stopPool();

  securityPtr->stopService();
    
  signal_fd_finalize();
  
  return EXIT_SUCCESS;
}
