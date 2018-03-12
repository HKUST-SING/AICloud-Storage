#pragma once


#include <mutex>
#include <queue>
#include <condition_variable>

namespace singaistorageipc
{


typename <class T>
class ConcurrentQueue
{

  public:

    /**
     * A simple single-producer single-consumer queue for passing 
     * tasks between two threads.
     */

    ConcurrentQueue();
    ~ConcurrentQueue();
    ConcurrentQueue(const ConcurrentQueue&) = delete;
    operator=(const ConcurrentQueue&) = delete;


    T pop();
    /**
     * remove the head of the queue and return it.
     */

    bool empty() const;
    /* If the queue empty.*/

    bool push(const T& item);
    /** 
     *
     * check if succesfully appended the item to 
     * the tail of the queue
     */
 

    bool push(T&& item);
    /**
     * Push as an rvalue.
     *
     */


  private:
    std::queue<T> queue_;
    std::mutex    mutex_;
    std::condition_variable cond_;



}; // ConcurrentQueue


// template class implementation

template <class T>
ConcurrentQueue<T>::ConcurrentQueue()
{
  
}


template <class T>
T ConcurrentQueue<T>::pop()
{
  std::unique_lock<std::mutex> tmp(mutex_);

  while(tasks._empty()) // wait until there is something to pop
  { 
    cond_.wait(tmp);
  }

  auto item = queue_.front()
  queue_.pop();

  return std::move(item);
}


template <class T>
bool ConcurrentQueue<T>::push(const T& item)
{
  std::unique_lock<std::mutex> tmp(mutex_);
  queue_.push(item);

  tmp.unlock();
  cond_.notify_one();

  return true;

}


template <class T>
bool ConcurrentQueue<T>::push(T&& item)
{
  std::unique_lock tmp(mutex_);
  queue_.push(std::move(item));
  
  tmp.unlock();
  cond_.notify_one();

  return true;
}


template <class T>
bool ConcurrentQueue<T>::empty() const
{
  // not thread-safe. 
  // Trying to access the queue without locking it.
  
  return queue_.empty();

}


} // namesapce singaistorageipc
