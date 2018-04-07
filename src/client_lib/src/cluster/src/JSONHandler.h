#pragma once

// C++ std lib
#include <sstream>
#include <exception>
#include <type_traits>
#include <list>
#include <utility>


// boost libraries
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/exceptions.hpp>


// Project libraries

// shorthand notation for boost property tree
namespace bpt = boost::property_tree;

namespace singaistorageipc{


class JSONResult
{
  /** The real worker class which reads a byte array
   * and tries to decode the content into a JSON
   * object.
   */


  template<typename>
  friend class JSONDecoder;
 
  void clearContent();
  void decodeData(std::stringstream& rawData);


  public:


    class JSONIter
    {

       // underlying system uses boost::property_tree
       using JSONTree = bpt::ptree; 
       using JSONItem = bpt::ptree::iterator;
       friend class JSONResult;

      public:

        JSONIter(const bool loopItr=false); //  default iterator
        JSONIter(JSONTree* obj, const bool loopItr=false);
        JSONIter(const JSONIter&);
        JSONIter(JSONIter&&); // move constructor


        ~JSONIter() = default;
      
        JSONIter getObject(const std::string& objKey);

        inline JSONIter getObject(const char* objKey)
        {
          return getObject(std::string(objKey));
        }


       JSONIter& operator*() 
       {
         return *this;
       } 

       JSONIter* operator->()
       {
         return this;
       }


       operator bool() const noexcept
       {
         return (objTree_ != nullptr);
       }

       // for iterating over all children
       JSONIter& operator++(); 
       JSONIter& operator++(int);

       JSONIter begin() const;     // get begining
       bool end() const noexcept;  // is iteration is over
       
       
       template<class T>
       T getValue(const std::string& key)
       {
         if(isLoop_)
         {
           return childItr_->second.get<T>(key);
         }

         return objTree_->get<T>(key);
       }

       template<class T>
       T getValue(const char* key)
       {
         if(isLoop_)
         {
           return childItr_->second.get<T>(key);
         }

         return objTree_->get<T>(key);
       }

       
      template<class T>
      void putValue(const JSONTree::key_type& key, const T& value)
      {
        if(isLoop_)
        {
          const bool endItr = end();
          if(!endItr)
          { // update the current node
            // find the child of the node
            auto valItr = childItr_->second.find(key);
            if(valItr != childItr_->second.not_found())
            {
              // just update the value
              valItr->second.put_value<T>(value);
            }
            else
            { // throw not found exception
              throw std::exception();  
            }// else
            
          }//if
          else
          {
            throw std::exception(); /// iterator is empty
          }
        }
        else
        {
          // means it's not a list iterator (not used in the loop)
          // look for children first
          auto valItr = objTree_->find(key);
          if(valItr == objTree_->not_found())
          { // update the node's data
            objTree_->put_value<T>(value);
          }
          else // one of the children
          {
            valItr->second.put_value<T>(value);
          }
        }
      }



      private:
        JSONTree*    objTree_;
        JSONItem     childItr_; 
        const bool   isLoop_;  // is this iterator used for looping
        
  

    }; // a wrapper class for easier object parsing 


    JSONResult() = default;
    ~JSONResult()
     {
       clearContent();
     }
    
    JSONIter getObject(const std::string& objName);    

    inline JSONIter getObject(const char* objName)
    {
      return getObject(std::string(objName));
    }   


  private:
    friend class JSONIter; // iterator should access the internal state 
    bpt::ptree objRoot_;  // root of the read JSON object
    

}; // class JSONResult


template<typename T>
class JSONDecoder 
{
  /**
   * Takes a char array and decodes the content into
   * a JSONObject. The JSONObject can be used for decoding
   * the passed data. Need to pass the buffer type in order
   * avoid using the same decoder with different data types
   * (char* vs uint8_t*).
   */


  public:

    /* Ensure a constant data buffer is created */
    using ConstBuffer = typename std::conditional<std::is_pointer<T>::value,                        std::add_pointer_t<std::add_const_t<std::remove_pointer_t<T>>>, std::add_const_t<T>>::type;


    JSONDecoder() = default;    
    ~JSONDecoder()
    {
      decData_.clearContent(); // clear the JSON conten
    }


   bool decode(ConstBuffer buffer)
   {
     // try to flush and decode the object content
     decData_.clearContent();

     // reuse the same object
     std::stringstream tmpStream(buffer); // initialize th stream
 
     bool resOp = true;

     try
     { // try to decode
       decData_.decodeData(tmpStream);
     }
     catch(const std::exception& exp)
     {
       resOp = false; // soemthing failed
     }

     return resOp;
   }


  const JSONResult& getResult() const noexcept
  {
    return decData_;
  }



  private:
    JSONResult  decData_; 
   
}; // class JSONDecoder


} // namesapce singaistorageipc
