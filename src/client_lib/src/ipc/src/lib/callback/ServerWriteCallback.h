#pragma once

#include <folly/io/async/AsyncTransport.h>
#include <folly/io/async/AsyncSocketException.h>

namespace singaistorageipc{

class ServerWriteCallback : public folly::AsyncWriter::WriteCallback{
public:
    ServerWriteCallback(int fd):fd_(fd){};
    /**
     * writeSuccess() will be invoked when all of the data has been
     * successfully written.
     *
     * Note that this mainly signals that the buffer containing the data to
     * write is no longer needed and may be freed or re-used.  It does not
     * guarantee that the data has been fully transmitted to the remote
     * endpoint.  For example, on socket-based transports, writeSuccess() only
     * indicates that the data has been given to the kernel for eventual
     * transmission.
     */
    void writeSuccess() noexcept override;

    /**
     * writeError() will be invoked if an error occurs writing the data.
     *
     * @param bytesWritten      The number of bytes that were successfull
     * @param ex                An exception describing the error that occurred.
     */
    void writeErr(size_t bytesWritten, 
        const folly::AsyncSocketException& ex) noexcept override;
private:
    int fd_;
};

}