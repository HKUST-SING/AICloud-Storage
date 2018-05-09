# -*- coding: utf-8 -*-


from __future__ import absolute_import
from __future__ import unicode_literals
from __future__ import division
from __future__ import print_function

# Dependencies
from builtins import int


# Python std lib
import uuid

# Generate a pseudo unique integer. The generator uses the
# the Python uuid  module for generating unique integers
# and returns the required length integers. 


def unique8():
	"""
		Generate a unique 8-bit length integer.
	"""
	return int(uuid.uuid1().int >> 120)


def unique16():
	"""
		Generate a unique 16-bit length integer.
	"""
	return int(uuid.uuid1().int >> 112)


def unique32():
	"""
		Generate a unique 32-bit length integer.
	"""
	return int(uuid.uuid1().int >> 96)


def unique64():
	"""
		Generate a unique 64-bit length integer.
	"""
	return int(uuid.uuid1().int >> 64)


def unique128():
	"""
		Generate a unique 128-bit length integer.
	"""
	return int(uuid.uuid4().int)
