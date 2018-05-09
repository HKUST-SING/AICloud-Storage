# -*- coding: utf-8 -*-


from __future__ import absolute_import
from __future__ import unicode_literals
from __future__ import division
from __future__ import print_function


# This file contains utility functions for initializing 
# internal stuctures and creating appropriate objects
# for data processing.


# Dependencies
from builtins import int



# singsotrage modules
import singstorage.internal.ipc as sing_ipc
from   singstorage.internal.utils.allocators.bfc import BFCAllocator


# constants used by the functions to instantiate 
# appropriate objects

# Module ID
USER_CTX = 0



# Dictionaries which are used to instantiate 
# the objects

# IPCs
__MOD_IPCS__ =\
	{
		USER_CTX	:	sing_ipc.SocketIPC
	}


# Memory allocators
__MOD_MEM_ALLOCS__ =\
	{
		USER_CTX	:	BFCAllocator
	}


def create_memory_allocator(mod_type, *args, **kwargs):
	MemAllocator = __MOD_MEM_ALLOCS__.get(mod_type, None)

	if MemAllocator: # found a class
		return MemAllocator(*args, **kwargs)
	else:
		raise ValueError("'{0}' module value is not available.".format(mod_type))



def create_ipc_process(mod_type, *args, **kwargs):

	IPCMethod = __MOD_IPCS__.get(mod_type, None)

	if IPCMetdod: # found a class
		return IPCMethod(*args, **kwargs)
	else:
		raise ValueError("'{0}' module value is not available.".format(mod_type))

