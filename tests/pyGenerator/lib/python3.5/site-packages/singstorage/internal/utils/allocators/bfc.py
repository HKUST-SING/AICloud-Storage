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
import collections


# singstorage modules
from singstorage.internal.utils.memory      import MemoryAllocator
from singstorage.internal.utils.memory		import log2_floor_eff 
from singstorage.internal.utils.collections import ListSet


class BFCAllocator(MemoryAllocator):
	"""
		A memory allocator which handles memory 
		as a set of bins and chunks (bins contain chunks).
		
		(For copyright and more refer to
		 www.github.com/HKUST-SING/AICloud-Storage/blob/master/src/client_lib/src/ipc/include/ipc/lib/utils/BFCAllocator.h)
	"""


	# C/C++ type constants for invalid values
	kInvalidChunkId 	= int(-1) # negative
	kInvalidBinId   	= int(-1) # negative
	kInvalidOffset		= int(-1) # negative
	kMinAllocationBits	= int(8)
	kMinAllocationSize	= int(1 << kMinAllocationBits)
	kNumOfBins			= int(21)



	class MemoryChunk(object):
		"""
			This class is an exact copy of the MemoryChunk 
			from the above github link.
		"""
		def __init__(self):
			self.is_free = True
			self.offset  = 0
			self.size    = 0
			self.prev_id = 0
			self.next_id = 0
			self.bin_num = 0


	class MemoryBin(object):

		def __init__(self, alloc, bin_size):
			self._bin_size = bin_size
			self._alloc    = alloc
			self._free_chunks = ListSet(int, self) 
			


		def insert_memory_chunk(self, chunk_id):
			self._free_chunks.insert(chunk_id)


		def remove_memory_chunk(self, chunk_id):
			self._free_chunks.erase(chunk_id)

		def find_chunk_id_larger_than_size(self, req_size):
			
			# iterate over set
			for id_val in self._free_chunks:
				chunk_ref = self._alloc.get_chunk(id_val)
				
				if chunk_ref.size >= req_size:
					return id_val

			# haven't found a suitable chunk
			return BFCAllocator.kInvalidChunkId


		def clear_bin(self):
			self._bin_size = 0
			self._free_chunks.clear()
			self._free_chunks = None
			self._alloc = None


		def __call__(id_one, id_two):
			
			chunk_one = self._alloc.get_chunk(id_one)
			chunk_two = self._alloc.get_chunk(id_two)

			if chunk_one.size != chunk_two.size:
				if chunk_one.size < chunk_two.size:
					return -1
				else:
					return 1

			else:
				if chunk_one.offset < chunk_two.offset:
					return -1
				elif chunk_one.offset > chunk_two.offset:
					return 1
				else:
					return 0



	class ResultChunk(MemoryAllocator.MemoryChunk):
		def __init__(self, total_size, 
					 data_chunks, chunk_ids,
					 chunk_key):

			super(MemoryChunk, self).__init__(chunk_key)	
			
			self._init_size = total_size
			self._av_size   = total_size

			# before assigning the list, sort it
			data_chunks.sort(key=lambda tk: tk[1])
			self._data_chunks = data_chunks
			self._chunk_ids = chunk_ids # chunk ids used by this block


		def get_available_size(self):
			self._av_size


		def get_chunk_part(self, req_size=0):

			if not self._data_chunks or\
					req_size > self._data_chunks[-1][1]:
				return None


			# must find a data chunk
			res_tuple = None

			if req_size > 0:    # means need to find
								# at least that big chunk	

				chunk_idx = self._find_chunk_idx(req_size)
				res_tuple = tuple(self._data_chunks[chunk_idx])
				del	self._data_chunks[chunk_idx]
				

			else: # return the smallest one
				res_tuple = tuple(self._data_chunks[0])
				self._data_chunks.pop(0)
			
			# update the remaining data size
			self._av_size -= res_tuple[1]

			return res_tuple


		def get_max_contiguous_size(self):
			# sorted list of chunks
			if  not self._data_chunks:
				return 0

			else:
				return self._data_chunks[-1][1]
			

		def get_total_size(self):
			return self._init_size


		def release_chunk(self, par_alloc):

			for chunk_id in self._chunk_ids:
				par_alloc.return_chunk(chunk_id)

			# reset all values
			self._init_size = 0
			self._av_size   = 0
			self._chunk_ids = None
			self._data_chunks = None

			par_alloc = None # do not hold the reference
							 # to the allocator


		def _find_chunk_idx(self, min_size):
		
			# must find a chunk
			d_arr   = self._data_chunks 
			beg_idx = 0
			end_idx = len(d_arr) - 1
			mid_idx = 0
			

			# do a slightly modified binary
			# search
			while beg_idx <= end_idx:
				mid_idx = beg_idx + (end_idx - beg_idx) // 2
		
				if d_arr[mid_idx][1] >= min_size and\
					mid_idx > 1 and\
					d_arr[mid_idx - 1][1] < min_size:
					# a good match
					return mid_idx

				elif d_arr[mid_idx][1] < min_size:
					beg_idx = mid_idx + 1 # left side

				else:
					end_idx = mid_idx - 1


			# if this step has been reached
			# means need to return the smallest part
			return 0
					


	############ BFCAllocator Methods ##############
	def __init__(self, alloc_id, mem_size, mem_beg):

		tmp_size = self._rounded_bytes(mem_size)
		# round the allocated region to some value
		super(BFCAllocator, self).__init__(alloc_id, tmp_size, mem_beg)

		self._bins   = collections.deque()
		self._chunks = collections.deque()
		self._free_chunks = BFCAllocator.kInvalidChunkId
		self._alloc_size  = int(0) # allocated size

		self._user_chunks = {}  # Result chunks used 
								# by the caller



	def init_alloc(self):
		self._init_memory_bins()
		self._init_memory_chunks()


	
	def allocate(self, mem_size):
		"""
			If not possible to allocate,
			return None
		"""
		if mem_size <= 0:
			return None

		# allocate an instance of the ResultChunk
		rounded_size = self._rounded_bytes(mem_size)

		bin_num = self._bin_number_from_size(rounded_size)

		# get as many chunks from the bin as possible
		# to satisfy the requirement
		return self._create_result_chunk(bin_num, rounded_size)


	def deallocate(self, chunk_key):
		"""
			Another way of deallocating a chunk is
			to use the chunk key 
			(some internal struct returned by get_chunk_key)
		"""
		ref_chunk = self._user_chunks.get(chunk_key, None)

		if not ref_chunk:
			raise KeyError("Chunk key '{0}' is not valid.".format(\
														chunk_key))

		# deallocate the chunk
		ref_chunk.release_chunk(self)

		# update the available memory size
		self._alloc_size -= ref_chunk.get_total_size()

		del self._user_chunks[chunk_key]



	def get_total_available_size(self):
		return (MemoryAllocator.get_memory_size() - self._alloc_size)


	def clear_alloc(self):
		# clear all bins
		for bin_itm in self._bins:
			bin_itm.clear_bin()
		
		self._bins   = collections.deque()
		self._chunks = collections.deque()
		self._free_chunks = BFCAllocator.InvalidChunkId
		self._alloc_size  = int(0) # currently allocated size


		self._user_chunks = {}  # allocated user chunks
								# ResultChunks



	def return_chunk(self, chunk_id):
		self._free_and_maybe_coalesce(chunk_id)

	
	def _init_memory_bins(self):

		# using range as it works in Python 2 and Python 3
		for bin_num in range(0, BFCAllocator.kNumOfBins, 1):
			bin_size = self._size_from_bin_number(bin_num)
			self._bins.append(BFCAllocator.MemoryBin(self, bin_size))


	def _init_memory_chunks(self):

		# allocate chunks of memory
		chunk_id  = self._allocate_chunk()
		chunk_ref = self.get_chunk(chunk_id)

		# initialize an empty chunk
		chunk_ref.is_free = True
		chunk_ref.offset  = int(0)
		chunk_ref.size	  = MemoryAllocator.get_memory_size()
		chunk_ref.prev_id = BFCAllocator.kInvalidChunkId
		chunk_ref.next_id = BFCAllocator.kInvalidChunkId
		chunk_ref.bin_num = BFCAllocator.kInvalidBinNum 


		# assign the newly created chunk to a bin
		self._insert_free_chunk_into_bin(chunk_id)
		
		

	def _size_from_bin_number(self, bin_num):
		return (int(256) << bin_num);


	def _bin_number_from_size(self, size_val):
		tmp_val = (max(size_val, int(256))\
					>> BFCAllocator.kMinAllocationBits)

		return min(BFCAllocator.kNumOfBins - 1, log2_floor_eff(tmp_val))


	def _allocate_chunk(self):
		# nothing has been allocated yet?
		if(self._free_chunks == BFCAllocator.kInvalidChunkId):
			self._chunks.append(BFCAllocator.MemoryChunk())
			
			return int(len(self._chunks) - 1)
		else:
			tmp_id = self._free_chunks
			chunk_ref = self.get_chunk(tmp_id)
			self._free_chunks = chunk_ref.next_id
			
			return tmp_id


	def get_chunk(self, chunk_id):
		return self._chunks[chunk_id]


	def _insert_free_chunk_into_bin(self, chunk_id):
		chunk_ref = self.get_chunk(chunk_id)
		assert( chunk_ref.is_free and\
				chunk.bin_num == BFCAllocator.kInvalidBinNum)

		bin_num = self._bin_number_from_size(chunk_ref.size)
		bin_ref = self._bins[bin_num]
		chunk_ref.bin_num = bin_num
		# insert the chunk into the retrieved bin
		bin_ref.insert_memory_chunk(chunk_id)


	def _remove_free_chunk_from_bin(self, chunk_id):
		chunk_ref = self.get_chunk(chunk_id)
		assert( chunk_ref.is_free and\
				chunk_ref.bin_num != BFCAllocator.kInvalidBinNum)

		# retrieved a valid empty chunk
		bin_ref = self._bins[chunk_ref.bin_num]
		bin_ref.remove_memory_chunk(chunk_id)
		chunk_ref = BFCAlloctor.kInvalidBinNum


	def _find_chunk_offset(self, bin_num, rounded_size):
		
		# use range as it works in Python2 and 3
		for bin_idx in range(bin_num, BFCAllocator.kNumOfBins, 1):
			bin_ref = self._bins[bin_idx]
			
			# try to find the chunk in the current bin
			found_id = bin_ref.find_chunk_id_larger_than_size(rounded_size) 			
			if found_id == BFCAllocator.kInvalidChunkId:
				continue # try another bin

			chunk_ref = self.get_chunk(found_id)
			assert(chunk_ref.is_free)

			bin_ref.remove_memory_chunk(found_id)
			chunk_ref.bin_num = BFAllocator.kInvalidBinNum

			if(chunk.size >= (rounded_size << 1)):
				self._split_chunk(found_id, rounded_size)

			# mark the chunk as occupied
			chunk_ref.is_free = False
			self._alloc_size += rounded_size
			
			return (chunk_ref.offset, found_id)

		# haven't found anything
		return None	



	def free_and_maybe_coalesce(self, chunk_id):
		chunk_ref = self.get_chunk(chunk_id)
		
		assert( chunk_ref.is_free is False and\
				chunk_ref.bin_num == BFCAllocator.kInvalidBinNum)

		chunk_ref.is_free = True

		# need to find a way of inserting the
		# chunk back to a bin
		chunk_back_id = chunk_id
		
		if(chunk_ref.next_id != BFCAllocator.kInvalidChunkId):
			# cannot just append
			next_chunk = self.get_chunk(chunk_ref.next_id)
			
			if next_chunk.is_free:  # not allocated
									# can merge two chunks into one
				self._remove_free_chunk_from_bin(chunk_ref.next_id)
				self._merge_chunks(chunk_id, chunk_ref.next_id)


		# try to merge with the previous chunk too
		if chunk_ref.prev_id != BFCAllocator.kInvalidChunkId:
			prev_chunk = self.get_chunk(chunk_ref.prev_id)
			
			# try to merge two chunks
			if prev_chunk.is_free:
				chunk_back_id = chunk_ref.prev_id
				self._remove_free_chunk_from_bin(chunk_ref.prev_id)
				self._merge_chunks(chunk_id, chunk_ref.prev_id)


		# have merged as mant chunks as possible
		# insert one only
		self._insert_free_chunk_into_bin(chunk_back_id)


	def _delete_chunk(self, chunk_id):
		self._deallocate_chunk(chunk_id)


	def _deallocate_chunk(self, chunk_id):
		chunk_ref = self.get_chunk(chunk_id)
		chunk_ref.next_id = self._free_chunks
		self._free_chunks = chunk_id


	def _split_chunk(self, chunk_id, size_val):
		chunk_ref = self.get_chunk(chunk_id)

		# make sure it's free and invalid
		assert( chunk_ref.is_free and\
				chunk_ref.bin_num == BFCallocator.kInvalidBinNum)

		# allocate a new chunk and steal some
		# memory from the current chunk
		new_chunk_id = self._allocate_chunk()
		new_chunk    = self.get_chunk(new_chunk_id)
		new_chunk.offset  = chunk_ref.offset + size_val
		new_chunk.size    = chunk_ref.size - size_val
		chunk_ref.size    = size_val
		new_chunk.is_free = True 
		new_chunk.bin_num = BFCAllocator.kInvalidBinNum
		
		# mark as available for allocation
		self._alloc_chunks[new_chunk.offset] = new_chunk_id
		neighbour_id = chunk_ref.next_id
		chunk_ref.next_id = new_chunk_id
		new_chunk.next_id = neighbour_id
		new_chunk.prev_id = chunk_id
		
		# if the neighbour is a valid chunk
		# need to update it
		if(neighbour_id != BFCAllocator.kInvalidChunkId):
			chunk_neighbour = self.get_chunk(neighbour_id)
			chunk_neighbour.prev_id = new_chunk_id

		# insert the newly created chunk into a bin
		self._insert_free_chunk_into_bin(new_chunk_id)


	def merge_chunks(self, chunk_one, chunk_two):
		chunk_ref_one = self.get_chunk(chunk_one)
		chunk_ref_two = self.get_chunk(chunk_two)

		# need to remove one chunk and append its
		# size to the other one
		
		assert(chunk_ref_one.is_free and chunk_ref_two.is_free)

		tmp_id = chunk_ref_two.next_id
		chunk_ref_one.next_id = tmp_id
		
		if tmp_id != BFCAllocator.kInvalidChunkId:
			# need to update the next chunk
			neighbour_chunk = self.get_chunk(tmp_id)
			neighbour_chunk.prev_id = chunk_one

		# append size of the second chunk to the first one
		chunk_ref_one.size += chunk_ref_two.size

		self._delete_chunk(chunk_two) # finish merging



	def _create_result_chunk(self, bin_num, rounded_size):
		"""
			Return a simple Result Chunk.
		"""
		res_chunk = self._find_chunk_offset(bin_num, rounded_size)
		if not res_chunk:
			return None # cannot allocate

		# create a ResultChunk
		res_val = BFCAllocator.ResultChunk(rounded_size,
			[(MemoryAllocator.get_mem_addr() + res_chunk[0], 
				rounded_size)], (res_chunk[1],), res_chunk[1])

		# mark as in-use
		self._user_chunks[res_chunk[1]]	= res_val

		return res_val # return ResultChunk
			

	def _rounded_bytes(self, byte_size):
		# make sure to use int from builtins
		# as it makes the code compatible with Python 2
		# and Python 3
		return  int((BFCAllocator.kMinAllocationSize *\
				((byte_size + BFCAllocator.kMinAllocationSize - 1) // BFCAllocator.kMinAllocationSize)))
