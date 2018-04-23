
// C++ std lib
#include <cstdlib>
#include <iostream>


// Project lib
#include "cluster/Worker.h"
#include "cluster/CephContext.h"
#include "ipc/IPCServer.h"


#ifdef SING_ENABLE_LOG
//Google log
#include <glog/logging.h>
#endif


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


int main(const int argc, const char* argv[])
{

  enableLogging(argv[0]);

  return EXIT_SUCCESS;
}
