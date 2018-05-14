#pragma once

// C++ std lib
#include <atomic>
#include <utility>


// Project lib


namespace singaistorageipc
{
  
template<class T, class ProdSeq, const int elements=16>
class Disruptor
{
  /** 
   * Disruptor follows the Disruptor patterns for message
   * passing between threads. It uses an array of fixed size 
   * (size should be a power of 2) as the message buffer and 
   * sequence numbers for coordinating access rights. 
   * For more information, read the LMAX Technical paper
   * on the Disrutpor pattern at lmax-github.io.
   *
   */
  

  public:

    Disruptor()
    : conSeq_(0),
      prodSeq_(-1),
      size_(elements)
    {}

    Disruptor(const Disruptor&) = delete;
    Disruptor(Disruptor&&) = delete;
    Disruptor& operator=(const Disruptor&)  = delete;
    Disruptor& operator=(const Disruptor&&) = delete;

   

    inline T getItem(const int index) const
    {
      return buffer_[index]; // no index checking since 
                             // producer and conumser have 
                             // to deal with it.
    }


    inline void setItem(const T& item, const int index)
    {
      buffer_[index] = item; // must support operator=(const T&);
    }

   inline void setItem(T&& item, const int index)
   {
     buffer_[index] = std::move(item); // must support operator=(T&&);
   }

  

  public:
    std::atomic<int> conSeq_; // consumer sequence number
    ProdSeq         prodSeq_; // producer sequence number 
                              // may be atomic<int> or just int 

    const int         size_;  // buffer size (# of elements)

  private:
    T buffer_[elements];      // message buffer

}; // clas SingleProducerDisruptor


} // namespacesingaistorageipc
