#pragma once


namespace singaistorageipc
{


typename <class T, class Args...>
class Queue
{
  /**
   * An interface for all queues used interanlly in the project.
   * The implementation can choose its scenario (for simple storage,
   * or for inter-thread communication). The interface just a warapper
   * for all queue implementations in the project.
   */

  Queue() {}
  virtual ~Queue(){}

  virtual T& front() const = 0; 
  /**
   * Return refernece to the head of the queue.
   * (Do not pop).
   */

  virtual void pop() = 0;
  /**
   * remove the head of the queue.
   */

  virtual bool empty() const = 0;
  /* If the queue empty.*/

  virtual bool push(const T& value) = 0;
 /** 
  *
  * check if succesfully appended the item to 
  * the tail of the queue
  */
 

 virtual bool push(T&& value) = 0;
 /**
  * Push as an rvalue.
  *
  */


 
 //Queue<T> createQueue(const string& type, const Args& args)
 /**
  * Factory method for queues.
  */
/*
 {
   // only one type is supported so far
   return new ConcurrentQueue(args); 
 }
*/
};

} // namesapce singaistorageipc
