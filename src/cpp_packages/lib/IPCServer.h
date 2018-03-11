/* Can test
 * IPC server object.
 */
#pragma once

/**
 * External dependence
 */

/**
 * Internal dependence
 */
#include "lib/IPCContext.h"

namespace singaistorageipc{

class IPCServer final{
public:
	IPCServer(IPCContext context):context_(context){};

    void start();

private:
	IPCContext context_;
};

}
