# -*- coding: utf-8 -*-


from __future__ import absolute_import
from __future__ import unicode_literals
from __future__ import division
from __future__ import print_function


# This file introduces an interface for managing 
# a memory region. It follows BFCAllocator implementation
# provided by SING lab.


# Dependencies
import six
from builtins import int # sublass of long on Python 2


# Python std lib
import abc



# function for computing log2 efficiently
def log2_floor_eff(int_val):
	return (int_val.bit_length() - 1)



@six.add_metaclass(abc.ABCMeta)
class MemoryAllocator(object):
	"""
		Class provides an interface for memory
		allocators/managers. The interface is supposed to
		provide an abstract contract between the operations
		which access shared memory and the implementers.
	"""



	@six.add_metaclass(abc.ABCMeta)
	class MemoryChunk(object):
		"""
			An Interface for underlying return type for allocate
			A subclass of the memory chunk implements how
			multiple non-contiguous memory regions are
			handled.
		"""

		def __init__(self, loc_key):
			self._mem_key = loc_key


		@abc.abstractmethod
		def get_available_size(self):
			"""
				Total cumulative free size of the chunk.
			"""
			pass


		@abc.abstractmethod
		def get_chunk_part(self, req_size=0):
			"""
				@req_size: required size 
					(if req_size <= 0, return
					any available chunk) 
					(if cannot find that big, returns a None)
				Return a tuple (mem_addr, size)
			"""
			pass


		@abc.abstractmethod
		def get_max_contiguous_size(self):
			"""
				Return the maximum contiguous size
				possible to retrieve by calling get_chunk_part.
			"""
			pass


		def get_chunk_key(self):
			return self._mem_key
			

		def set_chunk_key(self, chunk_key):
			self._mem_key = chunk_key


	def __init__(self, alloc_id, mem_size, mem_beg):
		self._id       = alloc_id
		self._mem_size = mem_size if(mem_size >= 0) else 0
		self._mem_addr = mem_beg  if(mem_beg >= 0 ) else 0


	def get_alloc_id(self):
		return self._id


	def set_alloc_id(self, alloc_id):
		self._id = alloc_id


	def get_memory_size(self):
		return self._mem_size


	def set_memory_size(self, mem_s):
		"""
			Change memory size. 
			Throw exception is a negative value given
		"""
		if mem_s < 0:
			raise ValueError("Memory region size cannot be negative.")

		self._mem_size = mem_s


	def set_memory_addr(self, mem_addr):
		"""
			Set the starting memory address of the 
			passed value.
			Throw exception is negative value.
		"""
		if mem_addr < 0:
			raise ValueError("Memory starting address cannot be negative.")

		self._mem_addr = mem_addr


	def get_memory_addr(self):
		return self._mem_addr



	def init_alloc(self):
		pass


	
	@abc.abstractmethod
	def allocate(self, mem_size):
		"""
			If not possible to allocate,
			return None
		"""
		pass

	
	def deallocate_chunk(self, mem_chunk):
		"""
			One way of deallocating the memory
			is sending back the chunk
		"""
		self.deallocate(self, mem_chunk.get_chunk_key())


	@abc.abstractmethod
	def deallocate(self, chunk_key):
		"""
			Another way of deallocating a chunk is
			to use the chunk key 
			(some internal struct returned by get_chunk_key)
		"""
		pass


	@abc.abstractmethod
	def get_total_available_size(self):
		pass


	def clear_alloc(self):
		pass
