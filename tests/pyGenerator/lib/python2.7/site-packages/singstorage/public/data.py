# -*- coding: utf-8 -*-

from __future__ import absolute_import
from __future__ import unicode_literals
from __future__ import division
from __future__ import print_function


# This module contains read/write data abstractions
# interfaces for providing a user with easy ways of
# reading/writing data to a remote storage system.


# Dependencies
import six


# Python std lib
import abc


@six.add_metaclass(abc.ABCMeta)
class ReadData(object):
	"""
		A abstraction for reading data in a python
		module. The returned object is iterable.
	"""
	def __init__(self):
		self._step = 1 # number of bytes
	

	@abc.abstractmethod
	def get_data(self):
		"""
			Returns a reference to raw bytes read from
			a remote storage system. The returned object
			is iterable (usually either bytearray or
			array.array). One element of the object
			represents one byte of data.
			(Warning: this method may not
			return the number of bytes returned by
			get_total_size(). It returns a reference
			to the local buffer (available data to read
			at one call (get_current_size returns number
			of bytes available at one call)))
			
			Might throw an exceptionp
		"""
		pass



	@abc.abstractmethod
	def __iter__(self):
		"""
			In order to ensure that the data object is iterable,
			the read object has to implement the __iter__ method
		"""
		pass


	@abc.abstractmethod
	def __next__(self):
		"""
			Return the next item of the byte data or throw
			the StopIteration exception.
		"""
		pass


	def next(self):
		return self.__next__()


	@abc.abstractmethod
	def get_total_size(self):
		"""
			Return the total size of data that
			the read(path) operation will 
			eventually read.
		"""
		pass


	@abc.abstractmethod
	def get_current_size(self):
		"""
			Return the number of bytes available to
			process without any delay. 
			(Number of bytes available locally)
			
		"""
		pass


	@abc.abstractmethod
	def get_remaining(self):
		"""
			Return the number of bytes that have not
			been processed by the user. The number
			returned by this method is eqaul to the number
			of bytes that the user has not read yet from the 
			total size. In other words, if the user iterates or
			calls get_data, the reutrned bytes are considered as
			consumed by the user.
		"""


	def set_step_size(self, step):
		"""
			When iterating over the object,
			the step is used to the number
			of bytes to return per iteration.
			If local buffer is smaller than the 
			step, a smaller size is returned.
		"""
		if step > 0:
			self._step = step


	def get_step_size(self):
		return self._step

