// C++ std lib
#include <cstdlib>
#include <cstring>


// OpenSSL lib
#include <openssl/sha.h>



#define NUM_DIGITS   10
#define NUM_LETTERS  26
#define NUM_RANGES   3

namespace singaistorageipc
{


  /**
   * A simple hash function which takes a char array 
   * as its input and outputs a 256-bit (32-byte) length
   * digest.
   *
   *
   * @return: 32-byte (256-bit) string  for naming shared memory
   *          (need to deallocate after the use)
   */

  char* memName32(const char* message)
  {
    unsigned char* digest = new unsigned char [SHA256_DIGEST_LENGTH+1];

    // create a SHA256
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, message, std::strlen(message));
    SHA256_Final(digest, &ctx);

    // make sure that the returned value is either a letter or a digit
    for(decltype(SHA256_DIGEST_LENGTH) i = 0; i < SHA256_DIGEST_LENGTH; ++i)    {
      // need to check three ranges:
      // ['0'-'9'] and ['A'-'Z'] and ['a'-'z'] 
      if((digest[i] < '0' || digest[i] > '9') &&
         (digest[i] < 'A' || digest[i] > 'Z') &&
         (digest[i] < 'a' || digest[i] > 'z'))
      {
        // convert into a printable char
        int opt[NUM_RANGES];
        opt[0] = std::rand() % NUM_DIGITS  + static_cast<int>('0');
        opt[1] = std::rand() % NUM_LETTERS + static_cast<int>('A');
        opt[2] = std::rand() % NUM_LETTERS + static_cast<int>('a');

        const int randPick = std::rand() % NUM_RANGES; // pick one
        digest[i] = static_cast<char>(opt[randPick]);
      }

    }

    // complete the path requirements
    digest[SHA256_DIGEST_LENGTH] = '\0'; // terminate string
    digest[0] = '/'; // must start with a '/'
    

    // return a random name for memory
    return ((char*) digest);
  }


} // namesapce singaistorageaipc
