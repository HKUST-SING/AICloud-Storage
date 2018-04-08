#pragma once

#include <cstdint>

#include <folly/futures/Future.h>

#include "include/Task.h"

namespace singaistorageipc{

class WorkerPool{
public:
	/**
	 * Assign the task to a worker according to
	 * `task.workerID_`. If the `workerID_` is 0,
	 * assign to the worker based on the dispatching
	 * protocol.
	 */
	folly::Future<Task> sendTask(Task task);

	// Assign the task to specific worker
	folly::Future<Task> sendTask(Task task,uint32_t workerID);

	unsigned int getSize();
};

}