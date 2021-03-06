#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>


// Facebook folly
#include <folly/futures/Future.h>

// Project lib
#include "include/Task.h"
#include "remote/Security.h"
#include "cluster/Worker.h"

namespace singaistorageipc{

class WorkerPool{


  public:

    static const constexpr uint32_t DEFAULT_POOL_SIZE = 1;


    WorkerPool(const unsigned int poolId,
               const unsigned int poolSize=0);


    ~WorkerPool();

    /**
     * Initialize the pool.
     */

    bool initialize(const char* cephFile,
                    const std::string& userName,
                    const std::string& clusterName,
                    const uint64_t cephFlags,
                    const char* protocolType,
                    std::shared_ptr<Security> sec);

    /**
     * Terminate the pool.
     */

    void stopPool(); 


	/**
	 * Assign the task to a worker according to
	 * `task.workerID_`. If the `workerID_` is 0,
	 * assign to the worker based on the dispatching
	 * protocol.
	 */
	folly::Future<Task> sendTask(Task task);

	// Assign the task to specific worker
	folly::Future<Task> sendTask(Task task, const uint32_t workerID);

	inline uint32_t getSize()   const noexcept;
    inline uint32_t getPoolID() const noexcept;


    static std::shared_ptr<WorkerPool> createWorkerPool(
                                    const char*         poolType, 
                                    const unsigned int  poolID, 
                                    const unsigned int  poolSize = 0);


  private:
    std::unique_ptr<Worker> createWorker(const char* cephConf,
                                         const std::string& userName,
                                         const std::string& clusterPool,
                                         const uint64_t cephFlags,
                                         const char*     protocolType,
                                         std::shared_ptr<Security> sec,
                                         const uint32_t workerID) const;


  private:
    const uint32_t                        poolID_;
    bool                                  active_; // if active pool
    uint32_t                              poolSize_;
    std::vector<std::unique_ptr<Worker> > workers_;
    std::vector<std::thread>              threads_;

};

}
