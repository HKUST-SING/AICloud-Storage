#pragma once


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
    
    bool initialize(const char* confFile);
    /**
     * Initializes the ceph context. Must be called from the same thread
     * after construction.
     * 
     * @param: confile: path to Ceph configuration file.
     *
     * @return: true on succes, false on failure
     */

    bool connectToCluster();
    /** After initialization of the object, need to connect to
     *  the cluster.
     * 
     * @return: true on success, false on failure
     */


   inline  bool checkPoolExists(const string& poolName) const
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

   inline librados::IoCtx& getSendContext(const string& poolName)
   {
     
   }


  private:
    librados::Rados cluster_; // cluster handle (one at most)
    string:: clusterName_; // cluster name
    string:: accessName_;  // username used for accessing data in cluster
    uint64_t accessFlags_; // access flags (Ceph specific)
    std::map<std::string, librados::IoCtx> ioOps_; // IO handles 
                                                   // (one per pool)
    
}; // class CephContext


} // namesapce singaistorageipc
