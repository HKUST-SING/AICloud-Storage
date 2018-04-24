// C++ std lib


// boost libraries
#include <boost/property_tree/json_parser.hpp>

// Project libraries
#include "JSONHandler.h"



namespace singaistorageipc
{


void
JSONResult::clearContent()
{
  objRoot_.clear(); // clear the root object
}

void
JSONResult::decodeData(std::stringstream& rawData)
{
  // just try to read the string stream and decode it
  bpt::read_json(rawData, objRoot_);
}


JSONResult::JSONIter
JSONResult::getObject(const std::string& objname)
{

  // just do simple search
  bpt::ptree* objPtr = nullptr;
 
  // may throw exception 
  objPtr = &(objRoot_.get_child(objname));


  return JSONResult::JSONIter(objPtr);
}



JSONResult::JSONIter::JSONIter(const bool loopItr)
: objTree_(nullptr),
  isLoop_(loopItr)
{}


JSONResult::JSONIter::JSONIter(JSONTree* treeObj, const bool loopItr)
: objTree_(treeObj),
  isLoop_(loopItr)
{
  if(treeObj)
  {
    childItr_ = treeObj->begin();
  }
  
}

JSONResult::JSONIter::JSONIter(JSONIter&& other)
: objTree_(other.objTree_),
  childItr_(other.childItr_),
  isLoop_(other.isLoop_)
{
  other.objTree_ = nullptr;
}


JSONResult::JSONIter::JSONIter(const JSONIter& other)
: objTree_(other.objTree_),
  childItr_(other.childItr_),
  isLoop_(other.isLoop_)
{}


JSONResult::JSONIter&
JSONResult::JSONIter::operator=(const JSONIter& other)
{
  if(this != &other)
  {
    objTree_  = other.objTree_;
    childItr_ = other.childItr_;
    isLoop_   = other.isLoop_;
  }

  return *this;
}


JSONResult::JSONIter&
JSONResult::JSONIter::operator=(JSONIter&& other)
{
  if(this != &other)
  {
    objTree_  = other.objTree_;
    childItr_ = other.childItr_;
    isLoop_   = other.isLoop_;

    other.objTree_ = nullptr;
  }


  return *this;

}


JSONResult::JSONIter
JSONResult::JSONIter::begin() const
{
  // return an iterator to children
  return JSONIter(objTree_, true);
}


JSONResult::JSONIter&
JSONResult::JSONIter::operator++()
{

 if(objTree_ && (childItr_ != objTree_->end()))
 {
   ++childItr_;
 }


  return *this; 

}

JSONResult::JSONIter&
JSONResult::JSONIter::operator++(int)
{

  if(objTree_ && (childItr_ != objTree_->end()))
  {
    ++childItr_;
  }

  return *this; 

}

bool
JSONResult::JSONIter::end() const noexcept
{

  if(objTree_)
  {
    return (childItr_ == objTree_->end());
  }

  return true;

}

JSONResult::JSONIter
JSONResult::JSONIter::getObject(const std::string& objKey)
{


  // handle exceptions
  if(!objTree_)
  {
    return JSONIter(nullptr);
  }

  // look for the object
  JSONTree* treePtr = nullptr;
  
  // may throw exception
  treePtr = &(objTree_->get_child(objKey));

  return JSONIter(treePtr);

}

} // namesapce singaistorageipc
