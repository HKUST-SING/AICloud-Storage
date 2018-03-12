#pragma once

// C++ std lib
#include <map>
#include <string>
#include <cstdint>


// Ceph lib
#include <rados/librados.hpp>


namespace singaistorageipc
{

class CephContext
{
/**
 * Ceph context maintains state information to access a Ceph 
 * cluster. It is used by workers in order to handle IO 
 * operations from local machines to the Ceph cluster.
 * Each worker has its own instance of CephContext to avoid
 * state sharing.
 */



  public:
    CephContext() = delete; // no dfault constructor
    ~CephContext();
    CephContext& operator=(const CephContext&) = delete;
    CephContext(const CephContext&); // supports copy constructor
    CephContext(std::string&& userName, std::string&& clusterName, 
                const uint64_t flags);
    CephContext(const std::string& userName, 
                const std::string& clusterName, const uint64_t flags);
 
    
    bool initAndConnect(const char* confFile);
    /**
     * Initializes the ceph context. Must be called from the same thread
     * after construction. The method initialized the cluster handle
     * and tries to connect to the cluster.
     * 
     * @param: confile: path to Ceph configuration file.
     *
     * @return: true on succes, false on failure
     */


   inline  bool checkPoolExists(const std::string& poolName) const
   /**
    * Check if the pool name has already been added.
    *
    * @param: poolName: name of the pool in the Ceph cluster
    *
    * @return: true if the pool exists, false if not.
    */
   {
     return (ioOps_.find(poolName) != ioOps_.end());
   }



  inline librados::IoCtx& getPoolRadosContext(const std::string& poolName)
  {
    /** 
     * Only use this method if checking is done before.
     */

    return (ioOps_.find(poolName)->second);
  }



   inline bool addRadosContext(const std::string& poolName)
   {
    // such pool does not exists yet
     if(ioOps_.find(poolName) == ioOps_.end())
     {
       return connectRadosContext(poolName);
     }
     
     return false; // means there is such a rados context 
            
   }

 
  inline bool removeRadosContext(const std::string&  poolName)
  // remove the pol context from the current connections
  {
    auto itr = ioOps_.find(poolName);
    if(itr != ioOps_.end())
    { // proceed further
    
      closeRadosContext(poolName);
      ioOps_.erase(itr); // remove from the map

      return true;
    }


    return false; // no such pool
  }


  void closeCephContext();
  /**
   * Method is called when the user wants to close the cluster 
   * connection. After this call, all io operations fail.
   *
   */
    


  private:
    bool connectRadosContext(const std::string& poolName);
    /**
     * Tries to connect to a remote Ceph pool and if it succeeds,
     * adds the newly created context to the pool.
     *
     * @param: Ceph poolname
     *
     * @return: true on success, false on failure.
     */

    bool closeRadosContext(const std::string& poolName);
    /** 
     * Closes a rados contex. Need to explicitly remove 
     * from the map.
     * 
     * @param: ceph poolname
     *
     * @return: true on success, false in failure.
     */
   

  private:
    librados::Rados cluster_; // cluster handle (one at most)
    std::string clusterName_; // cluster name
    std::string accessName_;  // username used for accessing 
                              //data in cluster

    uint64_t accessFlags_;    // access flags (Ceph specific)
    std::map<std::string, librados::IoCtx> ioOps_; // IO handles 
                                                   // (one per pool)

    bool init_;           // flag if the cluster initialized
    
}; // class CephContext


} // namesapce singaistorageipc
