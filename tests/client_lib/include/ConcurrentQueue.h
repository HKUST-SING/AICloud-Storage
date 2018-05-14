#pragma once


#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>


namespace singaistorageipc
{


template <class T, class Container=std::vector<T>>
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
    ConcurrentQueue& operator=(const ConcurrentQueue&) = delete;



    /** 
     * Dequeue more than one item (Taks).
     *
     * @param: items : a container to store all items 
     */
    void dequeue(Container& items);



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



}; // class ConcurrentQueue
} // namespace singaistorageipc




namespace singaistorageipc
{

// template class implementation

template <class T, class Container>
ConcurrentQueue<T, Container>::ConcurrentQueue()
{
  
}

template<class T, class Container>
ConcurrentQueue<T, Container>::~ConcurrentQueue()
{

  if(!empty())
  {
    while(!queue_.empty())
    {
      queue_.pop();
    }
  }

}



template <class T, class Container>
T ConcurrentQueue<T, Container>::pop()
{
  std::unique_lock<std::mutex> tmp(mutex_);

  while(queue_.empty()) // wait until there is something to pop
  { 
    cond_.wait(tmp);
  }

  auto item = std::move(queue_.front());
  queue_.pop();

  return std::move(item);
}


template<class T, class Container>
void ConcurrentQueue<T, Container>::dequeue(Container& items)
{
  std::lock_guard<std::mutex> tmpLock(mutex_);

  if(empty())
  {
    return;
  }

  // deque all possible queue items
  
  while(!queue_.empty())
  {
    items.push_back(std::move(queue_.front()));
    queue_.pop();
  }

}


template <class T, class Container>
bool ConcurrentQueue<T, Container>::push(const T& item)
{
  std::unique_lock<std::mutex> tmp(mutex_);
  queue_.push(item);

  tmp.unlock();
  cond_.notify_one();

  return true;

}


template <class T, class Container>
bool ConcurrentQueue<T, Container>::push(T&& item)
{
  std::unique_lock<std::mutex> tmp(mutex_);
  queue_.push(std::move(item));
  
  tmp.unlock();
  cond_.notify_one();

  return true;
}


template <class T, class Container>
bool ConcurrentQueue<T, Container>::empty() const
{
  // not thread-safe. 
  // Trying to access the queue without locking it.
  
  return queue_.empty();

}


} // namesapce singaistorageipc
