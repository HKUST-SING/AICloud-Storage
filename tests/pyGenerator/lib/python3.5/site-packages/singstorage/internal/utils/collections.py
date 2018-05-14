# -*- coding: utf-8 -*-

from __future__ import absolute_import
from __future__ import unicode_literals
from __future__ import division
from __future__ import print_function


# Dependencies
from builtins import int
import six


# Python std
import abc
import logging


# Local modules
import singstorage.utils.loc_logging as sing_log

##################### LOGGING #####################
# create a logger for this module
logger = logging.getLogger(__name__)

# create a console handler and set level to debug
ch = logging.StreamHandler()
ch.setLevel(sing_log.PACK_LOG_LEVEL)


# create a custom formatter
formatter = logging.Formatter("[%(levelname)8s] - %(name)s:%(lineno)s ---- %(message)10s")

ch.setFormatter(formatter)

# add handler to logger
logger.addHandler(ch)
logger.propagate=False

############ LOGGGING ENDS HERE ##############



@six.add_metaclass(abc.ABCMeta)
class Set(object):
	"""
		A abstract class for the data structure set.
		The interface provides a subset of the methods from the
		C++ standard library set interface.
	"""

	def __init__(self, data_type, comp_func):
		self._comparator = comp_func
		self._data_type  = data_type



	@abc.abstractmethod
	def insert(self, item):
		"""
			Try to insert a new item at the 
			position determined by the comparator.
			
			Return True on success, False on failure.
		"""
		pass


	@abc.abstractmethod
	def erase(self, item):
		"""
			Try to to erase the 'item' from the underlying
			container. 

			Return True on success, False on failure.
		"""
		pass




	@abc.abstractmethod
	def erase_at(self, item_idx):
		"""
			Try to erase the item at item_idx.
			
		"""
		pass


	@abc.abstractmethod
	def contains_val(self, item):
		"""
			Return false and a negative index
			if no item has been found.
		"""
		pass

	
	def __getitem__(self, item_idx):
		"""
			Implement only if the underlying container allows
			an efficient implementation.
		"""
		raise NotImplementedError("Set implementation does not support slicing.")



	@abc.abstractmethod
	def __iter__(self):
		"""
			Set must be iterable.
		"""
		pass


	def __bool__(self):
		"""
			By default return False.
		"""
		return False


	def __nonzero__(self):
		return self.__bool__()


	@abc.abstractmethod
	def __len__(self):
		"""
			Implement the length method.
		"""
		pass



	@abc.abstractmethod
	def empty(self):
		"""
			Implement traditional empty method.
		"""
		pass



	@abc.abstractmethod
	def clear(self):
		"""
			Implement traditional clear method.
			After clearing, the set must be reusable with 
			the same data type.
		"""
		pass



	def _compare(self, item_one, item_two):
		return self._comparator(item_one, item_two)


	def _check_type(self, item):
		return isinstance(item, self._data_type)


	def __str__(self):
		pass


class ListSet(Set):

	"""
		Very simple and rather inefficient implementation 
		of a SortedSet.
	"""


	def __init__(self, data_type, comp_func):

		super(ListSet, self).__init__(data_type, comp_func)

		self._set_size  = int(0)
		self._data = []



	def insert(self, item):
		"""
			Return false if does not match 
			the data type
		"""
		tmpLogger = logging.getLogger(__name__)
		tmpLogger.info("ListSet::insert")
		if not super(ListSet, self)._check_type(item):
			return False


		idx_list = int(0)
		for d_cont in self._data:
			comp_res = super(ListSet, self)._compare(item, d_cont)

			if comp_res == 0:
				return False # key already exists

			elif comp_res < 0:
				# found the index
				break
				
			else: # means the new item is larger 
				  # 'd_cont'
				idx_list += 1
		
		# insert at the given position
		self._data.insert(idx_list, item)
		self._set_size += 1

		return True # success
	


	def erase(self, item):
		"""
			Return false if does not match 
			the data type
		"""
		if not super(ListSet, self)._check_type(item) or\
				not self._data:
			return False


		idx_list = int(0)
		for d_cont in self._data:
			comp_res = super(ListSet, self)._compare(item, d_cont)

			if comp_res == 0:
				break # found the item

			idx_list += 1
		
		# insert at the given position
		if idx_list < self._set_size:
			del self._data[idx_list]
			self._set_size -= 1

			return True # deleted
		else:
			return False # cannot delete


	
	def erase_at(self, idx_val):
		"""
			Return false if idx_val 
			is not in the range
		"""
		if not self._data or idx_val < 0 or idx_val >= self._set_size:
			return False # cannot delete

		del self._data[idx_val]
		self._set_size -= 1

		return True



	def contains_val(self, item):
		"""
			Retun false if not
			of the type that stores
		"""	
		if not super(ListSet, self)._check_type(item) or\
				not self._data:
			return (False, int(-1))
	


		# using non-pythonic way so that it could work
		# with Python 2 and Python 3
		idx_list = 0

		for d_cont in self._data:
			comp_res = super(ListSet, self)._compare(item, d_cont)

			if comp_res == 0:
				return (True, idx_list) # found the item

			idx_list += 1

		return (False, int(-1))



	def __getitem__(self, item_idx):
		if item_idx < 0 or item_idx >= len(self._data):
			raise IndexError("{0} value is not in the range of data.".format(item_idx))

		return self._data[item_idx]


	def __iter__(self):
		return iter(self._data)


	def __bool__(self):
		return self._set_size != 0


	def __len__(self):
		return self._set_size


	def __str__(self):
		return str(self._data)


	def empty(self):
		return self._set_size == 0


	def clear(self):
		self._data = []
		self._set_size   = int(0)
