# -*- coding: utf-8 -*-

# This file contains an abstraction and implementation of a RADOS 

# Gateway Objects for the Python side. Each time the user writes/reads
# an object, this class encapsulates the operation and data. The file
# is useful since it provides an encapsulation for very large files.

from __future__ import absolute_import
from __future__ import unicode_literals
from __future__ import division
from __future__ import print_function


# Dependency packages
from builtins import int


# Python std lib
import logging
import mmap

# singstorage packages
import singstorage.public.data          as sing_data
import singstorage.utils.loc_logging    as sing_log
import singstorage.internal.messages	as sing_msgs
import singstorage.singexcept			as sing_errs

################ LOGGING ##################
# create a logger for this module
logger = logging.getLogger(__name__)

# create console handler and set level to debug
ch     = logging.StreamHandler()
ch.setLevel(sing_log.PACK_LOG_LEVEL) # set to only handle some logs

# create a custom formatter
formatter = logging.Formatter("[%(levelname)8s] - %(name)s:%(lineno)s ---- %(message)10s")

# add formatter to ch
ch.setFormatter(formatter)

# add handler to logger
logger.addHandler(ch)
logger.propagate=False

####### LOGGING ENDS HERE #############


# try to operate on the size of pages
# also, don't forget that the system uses Rados
# so max size is 4GB
__MAX_OP_SIZE__ = int(10*mmap.PAGESIZE)


class OpContext(object):
	"""
		Operation context class which contains the 
		most essential properties for any IO operation.
	"""

	# Operation type constants
	OP_READ    =  int(1)
	OP_WRITE   =  int(2)
	OP_DELETE  =  int(3)
	OP_CLOSE   =  int(4)


	def __init__(self, op_type, dpath, props=None):
		self._op     =  op_type # operation type
		self._path   =  dpath
		self._props  =  props
		self._result =  None 


	def get_op_type(self):
		return self._op


	def get_path(self):
		return self._path	


	def get_rados_properties(self):
		"""
			Return a reference to the storage properties
			which define how the object is stored.
		"""
		return self._props


	def set_op_type(self, op_type):
		self._op = op_type


	def set_path(self, dpath):
		self_path = dpath

	
	def set_rados_properties(self, dprops):
		self._props = dprops


	def get_result(self):
		return self._result


	def set_result(self, op_res):
		self._result = op_res


class WriteContext(OpContext):
	"""
		Interface for write operations. 
	"""
	def __init__(self, dpath, data=None):
		super(WriteContext, self).__init__( OpContext.OP_WRITE,
											dpath)
		self._raw_data = data # bytes of data
		self._len	   = int(len(data)) if(data) else int(0)
		self._offset   = int(0) # current write offset of data

	
	def get_write_data(self, data_size=None):
		"""
			Return memory view for efficient writing
		"""
		if self.is_completed():
			return b"" # an empty string

		cur_offset = self._offset

		# need to create a memory view and slice it
		# instead of slicing the bytearray
		raw_mem = memoryview(self._raw_data)

		if data_size and isinstance(data_size, int):
			
			end_idx = cur_offset + data_size

			if (end_idx < self._len):
				return raw_mem[cur_offset:end_idx:1]
			
			else: # return what's left
				return raw_mem[cur_offset::1]		
		
		else: # return the entire buffer
			return raw_mem[cur_offset::1] # avoid memory copies



	def update_data_offset(self, bytes_written):
		"""
			To know how many bytes have been written.
		"""
		# need to ensure that offset does not
		# overflow

		if self.is_completed():
			return # nothing to update


		if self._offset + bytes_written > self._len:
			self._offset = self._len

		else:
			self._offset += bytes_written



	def is_completed(self):
		return (self._offset == self._len)


	def get_write_size(self):
		return (self._len - self._offset)


	

class ReadContext(OpContext, sing_data.ReadData):
	"""
		Class implements the read abstraction object. It handles
		all the reading needed to read data from a Ceph
		cluster.
	"""


	def __init__(self, data_path):

		# multiple inheritance
		OpContext.__init__(self, OpContext.OP_READ, data_path)
		sing_data.ReadData.__init__(self)

		self._idx        =   0  # index of local reading
		self._len        =   0  # length of current buff

		self._data		 =   None
		self._read_op    =   None # a callback for more data


	def __iter__(self):
		return self


	def __next__(self):

		while True:

			if self._data and self._idx < self._len:
				tmp_off  = self._idx
				tmp_step = sing_data.ReadData.get_step_size(self)
  
				if tmp_step + tmp_off >= self._len:
					self._idx = self._len
				else:
					self._idx += tmp_step

				# avoid copying
				raw_view = memoryview(self._data)

				return raw_view[tmp_off:self._idx:1]


			# need to retrieve more data from
			# the underlying storage system

			self._retrieve_data()


	# make Python 2 and Python 3 work in the same way.
	next = __next__

	

	def get_remaining(self):
		raise NotImplementedError("Does not support get_remaining")


	def get_current_size(self):
		return (self._len - self._idx)

	
	def get_total_size(self):
		raise NotImplementedError("Does not support get_total_size")

	
	def get_data(self):

		if self.get_current_size() == 0:
			# return an empty array
			bytearray(0)


		tmp_off   = self._idx
		self._idx = self._len # completed

		# return memory view to avoid memory copies
		raw_mem = memoryview(self._data)

		return raw_mem[tmp_off::1]
			

	def set_read_data(self, read_data, read_callback):
		assert self._data is None and self._read_op is None, "set_read_data called multiple times"
		self._data      = read_data
		self._read_op   = read_callback



	def _close_read_operation(self):
		self._read_op.close_op()
		self._read_op = None


	def _retrieve_data(self):

		assert self._read_op is not None, "read_op is None"

		# try reading some data
		read_data = self._read_op.read_remote_data(self, 
												__MAX_OP_SIZE__)
		
		# check the result
		if isinstance(self.get_result(), sing_errs.StoreOpError):
			self._close_read_operation()
			raise self.get_result()

		if self.get_result() is sing_msgs.STAT_SUCCESS:
			# close the read operation
			self._close_read_operation()
			raise StopIteration() # means finished all data
									

		# otherwise, it means that still need to call for more data


		# after completing the read operation, 
		# update the internal structures
		self._data    = read_data
		self._len     = len(read_data)
		self._idx     = 0 # reset the index



	def __del__(self):
		"""
			Must implemnte the __del__ 'magic'
			method to ensure that all resources
			all properly deallocated.
		"""
		# make sure to close the read operation
		if self._read_op:
			self._close_read_operation()
		
		

class DeleteContext(OpContext):
	"""
		Class is now used for storing context
		for deleting an object.
	"""
	def __init__(self, dpath):
		super(DeleteContext, self).__init__(OpContext.OP_DELETE,
											dpath)


class CloseContext(OpContext):
	"""
		Context for closing the connection.
	"""								
	def __init__(self):
		super(CloseContext, self).__init__( OpContext.OP_CLOSE,
											"")
