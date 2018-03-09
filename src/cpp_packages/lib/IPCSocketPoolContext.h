/* Can test
 * Structure that store the socket context, like socket address,
 * amount of threads and etc..
 */

#pragma once

/**
 * External dependence
 */


/**
 * Internal dependence
 */
//#include "RequestHandlerFactory.h"

namespace singaistorageipc{

/**
 * Configure context for IPC server
 *
 * TODO: provide a conversion from a structured filed, ex. json,
 * to this configuration directly.
 */
class IPCSocketPoolContext{
public:

	/**
     * Number of threads to start to handle requests for each socket. 
     * Note that this excludes
     * the thread you call `IPCServer.start()` in.
     */
	size_t handler_threads{1};

    /**
     * Number of threads to listening port.
     * 
     * Now, we will try to handle all the socket in one thread.
     * SO the `listener_threads` shoule always be 1.
     */
    size_t listener_threads = 1;

	/**
     * Chain of RequestHandlerFactory that are used to create RequestHandler
     * which handles requests.
     *
     * Chain length may only be 1 now, but we keep the chain feature for
     * future development.
     */
    //std::vector<std::unique_ptr<RequestHandlerFactory>> handlerFactories;

    /**
     * This idle timeout serves following purpose -
     *
     * 1. How long to keep idle connections around before throwing them away.
     *
     * 1 minute as default
     */
    std::chrono::milliseconds idleTimeout{60000};

    /**
     * Maximum connection of each socket.
     *
     * 32 as default
     */
    uint32_t listenBacklog{32};

};

}
