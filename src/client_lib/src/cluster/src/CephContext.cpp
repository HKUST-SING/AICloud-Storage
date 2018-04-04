// C++ std lib
#include <utility>


// Project lib
#include "cluster/CephContext.h"


namespace singaistorageipc
{

using std::string;


CephContext::CephContext(const char* conf, const string& userName, 
                         const string& clusterName,
                         const uint64_t flags)
: clusterName_(clusterName), 
  accessName_(userName),
  confFile_(conf),
  accessFlags_(flags),
  init_(false)
{}


CephContext::CephContext(const char* conf, string&& userName, 
                         string&& clusterName,
                         const uint64_t flags)
: clusterName_(std::move(clusterName)), 
  accessName_(std::move(userName)),
  confFile_(conf),
  accessFlags_(flags),
  init_(false)
{}


CephContext::CephContext(const CephContext& other)
: clusterName_(other.clusterName_),
  accessName_(other.accessName_),
  confFile_(other.confFile_),
  accessFlags_(other.accessFlags_),
  init_(false)
{}



CephContext::~CephContext()
{
  if(init_) // check if the context needs to be closed
  {
    closeCephContext();
    init_ = false;
  }

  // reset the configuration file
  confFile_ = nullptr; 

}


bool
CephContext::initAndConnect()
{

  if(init_) // already initialized
  {
   return true;
  }

  // try first to initialize the rados handle
  int ret = cluster_.init2(accessName_.c_str(), clusterName_.c_str(),
                           accessFlags_);



  // if cannot initialize the handle, return false
  if(ret < 0)
  {
    return false; // cannot initialize the handle
  }


  // read a Ceph configuration file to configure the cluster handle
  ret = cluster_.conf_read_file(confFile_);
  
  if(ret < 0)
  {
    cluster_.shutdown(); // close the handle first
    return false;
  }


  // try to connect to the cluster
  ret = cluster_.connect();
  
  if(ret < 0)
  {
    cluster_.shutdown(); // close the cluster
    return false;
  }
  

  // mark the Ceph context as initliazed
  init_ = true;

  return true; // successfully initialized the cluster handle
}



void
CephContext::closeCephContext()
{
  if(!init_) // not initialized
  {
    return;
  }


  // first close all rados context instances
  for(const auto& value : ioOps_)
  {
    closeRadosContext(value.first);
  }

  // now clear the map
  ioOps_.clear();


  // close the handle to the cluster
  cluster_.shutdown();  


  // set as closed/non-initialized
  init_ = false;

}


bool
CephContext::connectRadosContext(const string& poolName)
{

  // check if connected
  if(!init_)
  {
    return false;
  }


  //create a librados io context
  librados::IoCtx tmpCtx;
  
  int ret = cluster_.ioctx_create(poolName.c_str(), tmpCtx);

  if(ret < 0)
  {
   // some error
   return false;
  }


  // add the newly created io context to the map
  auto tmpPair = std::make_pair<string, librados::IoCtx>(string(poolName), std::move(tmpCtx));
  
  auto res = ioOps_.insert(std::move(tmpPair));


  // if cannot insert the pool, close the context
  if(!res.second)
  {
    // cannot insert
    tmpCtx.close();
    return false;
  }
  

  return true; // all cases passed (successfully inserted)
 
}



bool
CephContext::closeRadosContext(const string& poolName)
{

  auto itr = ioOps_.find(poolName); // get reference to a pool

  itr->second.close(); // close the io context  

  return true;
}



} // namesapce singaistorageipc
