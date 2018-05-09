# -*- coding: utf-8 -*-

from __future__ import absolute_import
from __future__ import unicode_literals
from __future__ import division
from __future__ import print_function


# Python std lib
import hashlib


# singstorage packages
import singstorage.utils.encoding as encoding


def sha256_hash(value_to_hash):
	"""
		Utility function for hashing the passed string 
		to a 256-bit digest (SHA-256 used for hashing).
	"""

	return hashlib.sha256(
				   encoding.encode_to_bytes(value_to_hash)).digest()
