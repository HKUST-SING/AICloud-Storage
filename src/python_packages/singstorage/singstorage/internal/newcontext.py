# -*- coding: utf-8 -*-

from __future__ import absolute_import
from __future__ import unicode_literals
from __future__ import division
from __future__ import print_function


# The file contains the user's context (its properties and io ops).
# The class given in the file encamsulates the state that the API has
# to maintain per active user.


# Dependency modules
from   future.utils import viewitems as future_viewitems
import posix_ipc
from   builtins import int


# Python std lib
import os
import mmap
import logging


# singstorage modules
import singstorage.singexcept        	      as sing_errs
import singstorage.internal.ipc      		  as sing_ipc
import singstorage.internal.rados	 		  as sing_rados
import singstorage.utils.hash        		  as sing_hash
import singstorage.utils.unique_int_generator as sing_unique
import singstorage.internal.messages 		  as sing_msgs
import singstorage.utils.loc_logging 	      as sing_log
import singstorage.internal.utils.collections as sing_colls
from   singstorage import PYTHON_MAJOR_VERSION

# import the create module
import singstorage.internal.utils.create as sing_create


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




class StorageProperties(object):
	"""
		Class is a container for data processing properties.
		Properties are metadata for the stored data. For example,
		encoding, data compression, and so on.
	"""

	__PROPERTIES__ = {"encoding" : {"utf-8":True}}
 

	def __init__(self):
		self._vals = {"encoding" : "utf-8"}


	def _check_key(self, prop_key):
		"""
			Check if the property exists. If does not exist, throw
			an exception.
		"""
        
		res = StorageProperties.__PROPERTIES__.get(prop_key, None)
		if not res: # no such propery
			raise sing_errs.PropertyException(prop_key)

		return True



	def _check_val(self, prop_key, prop_val):
		"""
			Check if the property can be set to the pro_val
		"""
		# get property options
		options = StorageProperties.__PROPERTIES__.get(prop_key)
		if not options.get(prop_val, False): # cannot set to the value
			raise sing_errs.PropertyException(prop_key, prop_val, options)

		return True
            
    
	def get_property(self, prop_key):
		"""
			Get the current property value.
           
			Args: 
				prop_key : property key
          
			return: key value
			throw : exception if no property found  
		"""
		res = self._check_key(prop_key) # check if the key is fine


		# return the value of the given property
		return self._props[pop_key] 



	def set_properties(self, **kwargs):
		""" 
			Set data properties
		"""
		for prop_key, prop_val in future_viewitems(**kwargs):
			self._check_key(prop_key)
			self._check_value(prop_val)
			# can set the property
			self._props[prop_key] = prop_val

        


class UserContext(object):
	""" 
		Stores user context for smooth communication between the 
		Python package and the system serivce that serves data
		requests to/from the sorage cluster. 
	"""


	class __IOHandler__(object):
		"""
			A class which does all the work
			and which may run in a separate thread
			in later implementations.
		"""


		class PendOp(object):
			"""
				A class that presents an operation
				handle. It contains neccessry info
				for handling operations.
			"""

			OP_READ   = int(1)
			OP_WRITE  = int(2)
			OP_DELETE = int(3)
			OP_CLOSE  = int(4)

			def __init__(self, pend_op_type, completion_callback):
	
				self.op_type   = pend_op_type
				self.tran_id   = 0
				self.ipc_ref   = None
				self.active    = False # if it's active
				self.cmpl_call = completion_callback
				self.cmpl_flag = False


			def execute(self, op_ctx):
				raise NotImplementedError("PendOp is an interface.")


			def get_unique_id(self):
				return self.tran_id


			def set_unique_id(self, uq_id):
				self.tran_id = uq_id


			def get_ipc_handle(self):
				return self.ipc_ref


			def set_ipc_handle(self, ipc_handle):
				self.ipc_ref = ipc_handle


			def is_active(self):
				return self.active


			def set_active(self, act_op):
				self.active = act_op


			def is_completed(self):
				return self.cmpl_flag


			def get_op_type(self):
				return self.op_type

			def get_completion_callback(self):
				return self.cmpl_call


			def set_completion_callback(self, completion_callback):
				self.cmpl_call = completion_callback


			def close_op(self):
				# if there is completion callback,
				# use it 
				if self.cmpl_call: 
					self.cmpl_call(self.tran_id)
					self.cmpl_call = None

				self.tran_id   =  None
				self.ipc_ref   =  None
				self.active    =  False
				self.cmpl_flag =  True


		class PendRead(PendOp):
			"""
				Pending read operation.
			"""
			def __init__(self, mem_handle, completion_callback):
				super(UserContext.__IOHandler__.PendRead, self).__init__(
					UserContext.__IOHandler__.PendOp.OP_READ,
					completion_callback)
	   
				self.sh_mem    = mem_handle
		


			def read_remote_data(self, op_ctx, bytes_read):

				assert op_ctx, "read_remote_data has not received context"

				if self.is_completed() or bytes_read <= 0:
					op_ctx.set_result(sing_msgs.STAT_SUCCESS)
					return bytearray(0)


				# try to read data
				data_buffer = bytearray(0)  # buffer for storing read strings
				remote_bytes = 0

				while remote_bytes < bytes_read:
				
					read_data = self._issue_read(op_ctx, 0)
					
					if read_data is None:
						break

					else:
						remote_bytes += len(read_data)
						data_buffer.extend(read_data)


				# check if the operation completed
				if self.is_completed():
					self.close_op()

				return data_buffer



			def execute(self, read_ctx):

				tmp_logger = logging.getLogger(__name__)
				# execute gets called 
				# from the same thread, so avoid
				# acquiring the lock

				read_data = self._issue_read(read_ctx, 1)

				if read_data is None:
					tmp_logger.warn("PendRead::execute: read_data is None")
					read_ctx.set_read_data(None, None)
					self.close_op()
				else:
					read_ctx.set_read_data(bytearray(read_data), 
							self)



			def _issue_read(self, read_ctx, read_props):
				tmp_logger = logging.getLogger(__name__)
				tmp_logger.info("PendRead::_issue_read")


				# issue a READ request
				iss_op = self._send_msg(sing_msgs.MSG_WRITE,
										sing_msgs.MSG_READ, 
										data_path=read_ctx.get_path(),
										properties = read_props)

				if iss_op[0] is False: # exception occurred
					read_ctx.set_result(iss_op[1])
					self.close_op();
					tmp_logger.warn("PendRead::_issue_read: _send_msg failed.")
					return None


				# successfully sent and received a message
				res = iss_op[1]			

				# check the type of the received message
				if res.msg_type == sing_msgs.MSG_STATUS:
					tmp_logger.warn("PendRead::_issue_read: == MSG_STATUS")

					if res.op_status == sing_msgs.ERR_PATH:
						read_ctx.set_result(sing_errs.PathError(
								read_ctx.get_path(), False))

					elif res.op_status == sing_msgs.ERR_DENY:
							read_ctx.set_result(sing_errs.PathError(
								read_ctx.get_path(), True))

					else:
						tmp_logger.warn("MSG_STATUS: non-path related read")
						read_ctx.set_result(sing_errs.InternalError(sing_errs.INT_ERR_UNKNOWN))

					self.close_op() # close operation

					return None # no furhter processing

				else: # received the correct message
					if res.data_path != read_ctx.get_path():
						tmp_logger.error("PendRead::_issue_read: data_path != read_ctx.get_path()")
						read_ctx.set_result(sing_errs.InternalError(sing_errs.INT_ERR_PROT))
						self._send_msg( sing_msgs.MSG_STATUS,
										sing_msgs.MSG_READ,
										data_path=read_ctx.get_path(),
										properties = 2)					

						self.close_op() # close operation
						return None # no more processing

					assert res.msg_id == self.tran_id, "Not the same transaction IDs"

					# check if this response
					# means read is completed
					if self._check_read_completed(res):		
						# set read context as completed
						read_ctx.set_result(sing_msgs.STAT_SUCCESS)

						try:
							self.ipc_ref.send_request(sing_msgs.MSG_READ,
											self.tran_id,
											data_path=read_ctx.get_path(),
											properties=0)

						except Exception as exp:
							tmp_logger.warn("PendRead::_issue_read: {0}".format(exp))

						self.close_op() # done						

						return None
					
					else:
						# read data from the shared memory
						read_bytes, read_data = self._read_shared(res.mem_addr, res.data_length)

						if read_bytes != res.data_length:
							# an error, notify the service
							self._send_msg( sing_msgs.MSG_STATUS,
											sing_msgs.MSG_READ,
											data_path=read_ctx.get_path(),
											properties=2)

							tmp_logger.error("PendRead::execute: read_bytes != res.data_length")
							read_ctx.set_result(sing_errs.InternalError(sing_errs.INT_ERR_READ))

							self.close_op()

							return None # no further processing
						
						else:
							# successfully read
							# more data to read
							tmp_logger.info("PendRead::_issue_read: read bytes successfully.")
							read_ctx.set_result(sing_msgs.STAT_PART_READ)
							return read_data # return the data

							
				return None # default option is return None
					
				

			def _check_read_completed(self, ipc_msg):
				return (ipc_msg.mem_addr == 0 and ipc_msg.data_length == 0)


			def _read_shared(self, read_addr, read_len):

				# read the given number of bytes and
				# return a tuple: (read_bytes, data_string)
				result = (-1, None)
				try:
					data_len, data_read = self.sh_mem.read_data(read_addr, read_len)
					result = (data_len, data_read)
				except Exception as exp:
					tmp_logger = logging.getLogger(__name__)
					tmp_logger.warn("PendRead::read_shared: {0}".format(exp))
							
				
				return result
		

			def _send_msg(self, recv_type, msg_type, 
										**params):
				
				response = None	

				try:
					self.ipc_ref.send_request(msg_type, 
									self.tran_id, **params)

					res = self.ipc_ref.recv_request(recv_type)

					response = (True, res)
				except sing_errs.StoreOpError as exp:
					response = (False, exp)


				return response



		
			def close_op(self):

				# check if the operation has bee close
				# before
				if self.sh_mem is None and self.is_completed():
					return			

				self.sh_mem    = None

				super(UserContext.__IOHandler__.PendRead, self).close_op()



		class PendWrite(PendOp):
			
			def __init__(self, sh_handle, id_ref, completion_callback):
				super(UserContext.__IOHandler__.PendWrite, self).__init__(
					UserContext.__IOHandler__.PendOp.OP_WRITE,
					completion_callback)
				
				self.sh_mem     = sh_handle
				# a Set of lists: [seq_num, tran_id, buffsize, completed]
				self.issued_ops = sing_colls.ListSet(list,
										self._compare_seqs)

				self.seq_num    = int(1) # sequence number for ordering write messages 
										
				self.ocp_ids    = id_ref 



			def _compare_seqs(self, item_one, item_two):
				if item_one[0] == item_two[0]:
					return 0

				if item_one[0] < item_two[0]:
					return -1
				
				else:
					return 1




			def execute(self, write_ctx):
				"""
					Since the module is syncheonous at the time
					of writing, we write the entire write context
					to he shared memory and wait for the response
					from the storage service. Since the operation
					is active all this time, it is safe to
					reuse the same transaction id.
				"""
				if write_ctx.is_completed():
					write_ctx.set_result(sing_msgs.STAT_SUCCESS)
					self.close_op()

				else:
					# first step is to handle the write request
					# command
					if not self._start_write(write_ctx):
						self.close_op() # something went wrong
						return
					

					# need to process the data
					write_success = True    # need to check if write 
											# write succeeded
					while write_success and not write_ctx.is_completed():
						write_success = self._issue_write(write_ctx)

					# if successfully finished
					if write_success and write_ctx.is_completed():

						final_res = sing_msgs.STAT_SUCCESS


						try:
							while self.issued_ops:  # need to wait till all
													# writes complete
								self._wait_for_mem()

						except sing_errs.StoreOpError as exp:
							final_res = exp

						write_ctx.set_result(final_res)

					# either way need to close the operation
					self.close_op()
					# done processing


			def _send_msg(self, recv_type, msg_type, 
										**params):
				
				response = None


				# get a unique id
				tmp_id = sing_unique.unique32()
				
				while tmp_id in self.ocp_ids:
					tmp_id = sing_unique.unique32()
				
				# got a unique id
				self.ocp_ids[tmp_id] = True

						

				try:
					self.ipc_ref.send_request(msg_type, 
									tmp_id, **params)
						
					res = self.ipc_ref.recv_request(recv_type)

					response = (True, res)
				except sing_errs.StoreOpError as exp:
					response = (False, exp)

				# remove the message
				del self.ocp_ids[tmp_id]

				return response



			def _update_internal_refs(self, tran_id):

				assert tran_id in self.ocp_ids, "Received write transaction id , which does not exist in self.ocp_ids"

				del self.ocp_ids[tran_id]
				# find and mark the operation
				# as finished
				op_idx = 0
				for tmp_op in self.issued_ops:
					if tmp_op[1] == tran_id:
						break

					op_idx += 1 # for finding the completed write

				assert op_idx < len(self.issued_ops), "Transaction id does not  mathc any of the self.issued_ops in PendWrite."

				op_ref = self.issued_ops[op_idx]
				op_ref[-1] = True # mark as completed operation
						



			def _wait_for_mem(self):

				"""
					Wait for a read message to 
					release some memory.
				"""
				
				res = self.ipc_ref.recv_request(sing_msgs.MSG_READ)

				
				if res.msg_type == sing_msgs.MSG_STATUS:
					tmp_logger = logging.getLogger(__name__)
					tmp_logger.error("PendWrite::_wait_for_mem: received MSG_STATUS")
					# something went wrong
					# this only happens if some internal error
					raise sing_errs.InternalError(sing_errs.INT_ERR_UNKNOWN)

				# received a READ message
				assert res.msg_type == sing_msgs.MSG_READ or res.msg_type == sing_msgs.MSG_RELEASE, "Received unknown message type in _wait_for_mem: {0}".format(res.msg_type)

				# need to retrieve message id for mapping
				assert res.msg_id in self.ocp_ids, "Received READ message without referring to a WRITE"

				# check if the received message points to 
				# the very first write data msgs
				if self.issued_ops[0][1] == res.msg_id:
					# release all the memory
					self.sh_mem.release_mem(self.issued_ops[0][2])
					del self.ocp_ids[res.msg_id]
					self.issued_ops.erase_at(0)


					while self.issued_ops and self.issued_ops[0][-1]:
						# completed op
						del self.ocp_ids[self.issued_ops[0][1]]
						self.sh_mem.release_mem(self.issued_ops[0][2])
						self.issued_ops.erase_at(0)


				else:   # one of the internal writes
						# has completed first
					self._update_internal_refs(res.msg_id)	



			def _issue_write(self, write_ctx):
				"""
					Issue a write request to the shared memory
					and wait for response.
				"""
				tmpLogger = logging.getLogger(__name__)
				tmpLogger.info("PendWrite::_issue_write")

				# need to make sure that there
				# is available memory to write
				while self.sh_mem.avail_buffer() <= 0:
					assert self.sh_mem.avail_buffer() ==  0, "Negative write buffer size"
				
					op_res = True
						
					try:
						self._wait_for_mem()

					except sing_errs.StoreOpError as exp:
						write_ctx.set_result(exp)
						op_res = False

					if op_res is False:
						return False # something failed
		
				# there is some shared memory to use
				written = 0
				beg_addr = self.sh_mem.get_write_addr()
				data = write_ctx.get_write_data(self.sh_mem.avail_buffer())
				assert len(data), "PendWrite::_issue_write:Data write is empty"
					
				try:
					written = self.sh_mem.write_data(data)
				except Exception as exp:
					tmpLogger.error(exp)
					write_ctx.set_result(sing_errs.InternalError(\
								sing_errs.INT_ERR_WRITE))
					written = None

				# if written None, error
				if written is None:
					# some internal error
					self._send_msg( sing_msgs.MSG_STATUS,
									sing_msgs.MSG_WRITE,
									data_path=write_ctx.get_path(),
									properties=2, start_addr = int(0),
									data_length=int(0))	
		
					return False

				# approve the context
				write_ctx.update_data_offset(written)

				op_res = self._notify_service(beg_addr, written, write_ctx)

				# completed one write request
				return op_res
					
				


			def _notify_service(self, addr, write_len, write_ctx):
				

				res = True

				# create an item ot insert
				
				# generetate a unique op id
				op_tran_id = sing_unique.unique32()			

				while op_tran_id in self.ocp_ids:
					op_tran_id = sing_unique.unique32()

				# prepare the operation item
				tmp_item = [self.seq_num, op_tran_id, write_len, False]
				
				# update the sequence number for next op
				self.seq_num += 1

				# assume a success
				self.ocp_ids[op_tran_id] = True
				assert self.issued_ops.insert(tmp_item), "Cannot insert a new write operation/transaction"
				 
				try:
				
					self.ipc_ref.send_request(sing_msgs.MSG_WRITE, 
								op_tran_id,
								data_path=write_ctx.get_path(), 
								properties=0)

				
				except sing_errs.StoreOpError as exp:
					write_ctx.set_result(exp)
					res = False

					# undo the assumption
					del self.ocp_ids[op_tran_id]
					self.issued_ops.erase(tmp_item)
					self.seq_num -= 1 # move by one back


				return res 

				

			def _start_write(self, write_ctx):
				"""
					First message must be empty
					with the size to write.
				"""

				res = None
				try:
					self.ipc_ref.send_request(sing_msgs.MSG_WRITE, 
									self.get_unique_id(),
									data_path=write_ctx.get_path(),
									properties=1, start_addr=int(0),
									data_length=write_ctx.get_write_size())

					# wait for result of the write
					# from the service
					res = self.ipc_ref.recv_request(sing_msgs.MSG_READ)
				except sing_errs.StoreOpError as exp:
					res = None
					write_ctx.set_result(exp)

				if res is None:
					return False
				

				# need to check the message type
				if res.msg_type == sing_msgs.MSG_STATUS:
					# set an exception
					if res.op_status == sing_msgs.ERR_PATH:
						write_ctx.set_result(sing_errs.PathError(
							write_ctx.get_path(), False))

					elif res.op_status == sing_msgs.ERR_QUOTA:
						write_ctx.set_result(sing_errs.QuotaError(
							write_ctx.get_write_size(), 0))

					elif res.op_status == sing_msgs.ERR_DENY:
						write_ctx.set_result(sing_errs.PathError(
								write_ctx.get_path(), True))

					elif res.op_status == sing_msgs.ERR_PROT:
						write_ctx.set_result(sing_errs.ProtError(
							write_ctx.get_path()))

					else: # some internal error
						write_ctx.set_result(sing_errs.InternalError(
								sing_errs.INT_ERR_UNKNOWN))

					return False 	# no need to process
									# further the operation

				else: # must be a READ message
					assert res.msg_type == sing_msgs.MSG_READ, "Not READ msg"
					assert res.msg_id   == self.get_unique_id(), "Response message id does not match sent WRITE request id"
					assert res.prop_bitmap == 1, "READ bitmap unset"

					return True # write operation granted



			def close_op(self):
				
				# release all IDs
				for tmp_op in self.issued_ops:
					try:
						del self.ocp_ids[tmp_op[1]]
					except KeyError:
						pass # ignore if key is not found

				self.sh_mem      =  None
				self.ocp_ids     =  None
				self.issued_ops  =  None 

				super(UserContext.__IOHandler__.PendWrite, self).close_op()


		
		class PendDelete(PendOp):	

			def __init__(self, completion_callback):
				super(UserContext.__IOHandler__.PendDelete, self).__init__(
					UserContext.__IOHandler__.PendOp.OP_DELETE,
					completion_callback)
			


			def execute(self, del_ctx):
				tmp_logger = logging.getLogger(__name__)
				tmp_logger.info("PendDelete::execute")
				
				success = True

				try:
				
					self.ipc_ref.send_request(  sing_msgs.MSG_DELETE,
												self.tran_id,
												data_path=del_ctx.get_path())

					# wait for result
					res = self.ipc_ref.recv_request(sing_msgs.MSG_STATUS)

				except sing_errs.StoreOpError as exp:
					del_ctx.set_result(exp)
					success = False

				if success is False:
					self.close_op()
					return # some error occurred
				
				# check message type
				assert res.msg_type == sing_msgs.MSG_STATUS, "Delete receives some different message."
				assert res.msg_id == self.tran_id, "STATUS id != message id"

				# if stauts is success, set result and return
				if res.op_status == sing_msgs.STAT_SUCCESS:
					del_ctx.set_result(sing_msgs.STAT_SUCCESS)
			
				# all error types
				elif res.op_status == sing_msgs.ERR_PATH:
					del_ctx.set_result(sing_errs.PathError(
							del_ctx.get_path(), False))

				elif res.op_status == sing_msgs.ERR_DENY:
					del_ctx.set_result(sing_errs.PathError(
								del_ctx.get_path(), Trye))

				elif res.op_status == sing_msgs.ERR_QUOTA:
					del_ctx.set_result(sing_errs.QuotaError(0, 0))

				else: # some internal error
					del_ctx.set_result(sing_errs.InternalError(sing_errs.INT_ERR_UNKNOWN))


				# mark the operation as finished
				self.close_op()


			def close_op(self):
				super(UserContext.__IOHandler__.PendDelete, self).close_op()



		class PendClose(PendOp):
				
			def __init__(self, completion_callback):
					
				super(UserContext.__IOHandler__.PendClose, self).__init__(
					UserContext.__IOHandler__.PendOp.OP_CLOSE,
						completion_callback)


			def execute(self, close_ctx):
				tmp_logger = logging.getLogger(__name__)
				tmp_logger.info("PendClose::execute")
				

				# closing does not throw exceptions
				self.ipc_ref.close_conn()		
				close_ctx.set_result(sing_msgs.STAT_SUCCESS)
				# mark the operation as finished


				self.close_op()
							


			def close_op(self):
				super(UserContext.__IOHandler__.PendClose, self).close_op()



######################### __IOHandler__ Methods ################


		# maximum number of active IOs
		# need to restrict number of
		# active IOs due to memory constraints
		# and latency
		MAX_PEND_OPS = int(3)

		def __init__(self, ipc_handle, sh_read_mem,
						sh_write_mem):

			self.ipc_ref    = ipc_handle
			self.sh_read    = [sh_read_mem,  True] # shared read memory handle
			self.sh_write   = [sh_write_mem, True] # shared write memory handle 
			self.pend_ops   = []           # to be scheduled ops
			self.active_ops = {}           # active ops
			self.active_ids = {}           # map of active ids

			self.closed     = False        # if close message
											   # has been sent
											


		def _unsafe_discard(self, op_id):
			"""
				Callback for removing a completed
				operation from the active map.
			"""
			assert op_id in self.active_ids,\
					"Transaction ID is not in the map."

			# remove the operations
			dpath = self.active_ids[op_id]		
			op_ref = self.active_ops[dpath]

			if op_ref.get_op_type() ==\
					UserContext.__IOHandler__.PendOp.OP_READ:
				self.sh_read[1] = True

			if op_ref.get_op_type() ==\
					UserContext.__IOHandler__.PendOp.OP_WRITE:
				self.sh_write[1] = True


			del self.active_ops[dpath]
			del self.active_ids[op_id]



		def _safe_discard(self, op_id):
			"""
				Callback for safe way of discarding
				a completed operation.
			"""
			assert op_id in self.active_ids,\
					"Transaction ID is not in the map."			


			dpath  = self.active_ids[op_id]
			op_ref = self.active_ops[dpath]
					
			# release the shared memory
			# resources if acquired
			if op_ref.get_op_type()  ==\
					UserContext.__IOHandler__.PendOp.OP_READ:
				self.sh_read[1] = True

			if op_ref.get_op_type() ==\
					UserContext.__IOHandler__.PendOp.OP_WRITE:
				self.sh_write[1] = True

			del self.active_ops[dpath]
			del self.active_ids[op_id]



		def _close_callback(self, op_id):
			"""
				Callback used by close operations.
			"""
			pass


		def append_new_operation(self, op_ctx):

			if self.closed:
				return


			self.pend_ops.append(op_ctx)

			self._issue_new_op()

			# if it is possible to issue a new
			# operation, do so
					

		def _can_issue_op(self, op_type):
				
			if op_type == sing_rados.OpContext.OP_READ:
				return self.sh_read[1] # if it's free

			elif op_type == sing_rados.OpContext.OP_WRITE:
				return self.sh_write[1] # if write memory free

			else:
				return True
		 

		def _issue_new_op(self):
			# issue an IO operation
			if not self.pend_ops or self.closed:
				return

			tmp_logger = logging.getLogger(__name__)
			tmp_logger.info("IOHandler::_issue_new_op")

			# make sure multiple operations
			# to the same data item do not overlap
			op_idx = 0
					
			for av_op in self.pend_ops:
				# look for a non-active op
				if av_op.get_path() in self.active_ops:
					op_idx += 1
					continue

				else: # found one
					break

			# make sure there is a valid operation
			if op_idx == len(self.pend_ops):
				return
					

			# only one write and one read
			# are supported
			# don't share memory among multiple
			# among active writes/reads
			op_ctx  = self.pend_ops[op_idx]
			op_type = op_ctx.get_op_type()

			if not self._can_issue_op(op_type):
				return # cannot issue this operation

			op_ctx = self.pend_ops.pop(op_idx)
		
			# determine the type of the 
			# operation context
			op_type = op_ctx.get_op_type()
			new_op  = None

			if op_type == sing_rados.OpContext.OP_READ:
				self.sh_read[1] = False # mark as occupied	
				new_op = UserContext.__IOHandler__.PendRead(
											self.sh_read[0],
											self._safe_discard)


			elif op_type == sing_rados.OpContext.OP_WRITE:
				self.sh_write[1] = False # mark as occupied
				new_op = UserContext.__IOHandler__.PendWrite(
											self.sh_write[0], 
											self.active_ids,
											self._unsafe_discard)

			
			elif op_type == sing_rados.OpContext.OP_DELETE:
				new_op = UserContext.__IOHandler__.PendDelete(
											self._unsafe_discard)
			

			elif op_type == sing_rados.OpContext.OP_CLOSE:
				new_op = UserContext.__IOHandler__.PendClose(
											self._close_callback)
				self.closed = True # sending close (don't accept any more ops)
			
			else:
				tmp_logger.error("IOHandler::_issue_new_op: operation context has no type")
				assert False, "OP Context has no type" # terminate


			# a new operation is prepared
			# initialize the operation
			new_op.set_ipc_handle(self.ipc_ref)
			new_op.set_active(True)
		
			# get a unique op ID
			tmp_id = sing_unique.unique32()

			while tmp_id in self.active_ids:
				# generate a 32-bit integer and 
				# test if it's unique 
				tmp_id = sing_unique.unique32()
			
			# got a unique id
			new_op.set_unique_id(tmp_id)
			
			# insert the id and the data key 
			# into the dictionaries
			self.active_ids[tmp_id] = op_ctx.get_path()
			self.active_ops[op_ctx.get_path()] = new_op

		
			# execute the operation
			new_op.execute(op_ctx)
			# done with this operation
			



		def close_handler(self):
			"""
				Stop all operations.
			"""
			key_list = list(self.active_ops)

			for op_path in key_list:
				op_term = self.active_ops[op_path]
				op_term.close_op()


			# close the memory
			if self.sh_read[0] is not None:
				try:
					self.sh_read[0].close_mem()
				except Exception as exp:
					pass

				# set to None
				self.sh_read[0] = None
				self.sh_read[1] = False


			if self.sh_write[0] is not None:
				try:
					self.sh_write[0].close_mem()
				except Exception as exp:
					pass

				self.sh_write[0] = None
				self.sh_write[1] = False
		

			

			# release all allocated resources
			self.active_ops = {}
			self.active_ids = {}

			if self.ipc_ref.is_connected():
				self.ipc_ref.close_conn()

			self.closed = True
			

		def is_closed(self):
			return self.closed



	def __init__(self, username, password):
		self._user        =    username
							   
                               # use only a hash of the password
		self._passwd      =    sing_hash.sha256_hash(password) 
		self._props       =    StorageProperties()
		self._io_handler  =    None



	def _create_write_buf(self, mem_addr, mem_size):

		class SharedMemStruct(object):
			def __init__(self, start_addr, region_size):
				self._beg_addr = start_addr  # address to begin 
				self._mem_head = int(0)      # offset
				self._mem_tail = region_size # tail 
				self._mem_size = region_size # memory size
				self._fd_shm   = None
				self._mmap     = None


			def init_mem(self, mem_name, bufsize):
				self._fd_shm = posix_ipc.SharedMemory(
								mem_name, flags=0, mode=666, 
								size=bufsize, 
								read_only=False)

				self._mmap   = mmap.mmap(self._fd_shm.fd, 
								length=0, flags=mmap.MAP_SHARED,
								prot=mmap.PROT_READ | mmap.PROT_WRITE,
								access=mmap.ACCESS_WRITE,
								offset=0)


			def close_mem(self):
				self._mmap.close()
				self._fd_shm.close_fd()
				self._mem_head = self._mem_tail =\
				self._beg_addr = self._end_addr = None


			def can_close_mem(self): # check before closing 
                                     # if all pending data has been
                                     # approved by the storage service
				return True
                    


			def write_data(self, data):
				# check if there is available data
				if self._mem_head == self._mem_tail:
					return 0 # cannot write (full window)

              
                
				# move the file descriptor to the correct position
				self._mmap.seek(self._mem_head, 
								os.SEEK_SET)

				# check how many bytes it can be written 
				need_to_write = len(data) # this many bytes 
										  # has to be written
                
				avail = self.avail_contiguous() # get the 
												# available size

				# check how many bytes can be written in one call
				will_write = min(avail, need_to_write)

				# compute number of written bytes
				bytes_written = self._mmap.tell()

				# get memory view for efficient writing
				raw_bytes = memoryview(data)

				if PYTHON_MAJOR_VERSION == 3:
					
					start_idx = 0

					# issue multiple writes
					while start_idx < will_write:
						self._mmap.write(raw_bytes[start_idx:will_write:1])
						start_idx += (self._mmap.tell() - bytes_written)
			
				else: # almost same code (one if check vs multiple)

					start_idx = 0

					while start_idx < will_write:
						self._mmap.write(raw_bytes[start_idx:will_write:1].tobytes())
						start_idx += (self._mmap.tell() - bytes_written)
					
			
				
				bytes_written = (self._mmap.tell() - bytes_written)

				# update the head pointer to the new position
				self._mem_head = ((self._mem_head + bytes_written) % self._mem_size)

				return bytes_written
                

            
			def avail_buffer(self):
				if self._mem_head > self._mem_tail:
					return (self._mem_size - self._mem_head) +\
						   self._mem_tail
				else:
					return (self._mem_tail - self._mem_head)


			def avail_contiguous(self):
				if self._mem_head > self._mem_tail:
					return (self._mem_size - self._mem_head)
				
				else:
					return (self._mem_tail - self._mem_head)



			def get_write_addr(self):
				return (self._mem_head + self._beg_addr) 
									# return the memory address for
									# writing 



			def release_mem(self, buf_size): # data has been read by
                                             # the control service

				tmp_tail = ((self._mem_tail + buf_size) % self._mem_size)\
								- 1

				# to ensure that mem_tail is never negative
				self._mem_tail = max(tmp_tail, 0)


			def clear(self):
				"""
					Reset the memory region.
				"""
				self._mem_head = int(0)
				self._mem_tail = self.mem_size


		return SharedMemStruct(mem_addr, mem_size)



	def _create_read_buf(self, mem_addr, mem_size):

		class SharedMemStruct(object):
			def __init__(self, start_addr, region_size):
				self._beg_addr = start_addr # 
				self._end_addr = start_addr + region_size
				self._fd_shm   = None
				self._mmap     = None


			def init_mem(self, mem_name, bufsize):
				self._fd_shm = posix_ipc.SharedMemory(
								mem_name, 
								0, mode=444, 
								size=bufsize, read_only=False)

				self._mmap   = mmap.mmap(self._fd_shm.fd, 
								length=0,
								flags=mmap.MAP_SHARED,
								prot=mmap.PROT_READ,
								offset=0)


			def close_mem(self):
				self._mmap.close()
				self._fd_shm.close_fd()
				self._beg_addr = self._end_addr = None

                    

			def read_data(self, read_addr, read_len):
				# if the read address is out of the range
				# throw an exception
				tmp_logger = logging.getLogger()
				tmp_logger.info("SharedMemStruct::read_data")

				if  read_addr >= self._end_addr or\
					read_addr < self._beg_addr or\
					read_addr + read_len > self._end_addr:

					tmp_logger.error("Reading address ranges are wrong")
					raise sing_errs.InternalError(sing_errs.INT_ERR_PROT)

				# read the given number of bytes and 
				# return a tuple: (read_bytes, data_string)
				# move to the correct place of the file
				self._mmap.seek((read_addr - self._beg_addr), os.SEEK_SET)
                

				# try to read all the data
				bin_read_data = []
				final_data    = None
                left_read     = read_len

				while 1: 
					d_read     = self._mmap.read(read_len)
					left_read -= len(d_read)

					if left_read < 0:
						tmp_logger.error("read_data: read more data than required")
						raise sing_errs.InternalError(sing_errs.INT_ERR_READ)

					if left_read == 0 and not bin_read_data:
						# avoid copying data
						final_data = d_read
						break

					# combine multiple reads
					bin_read_data.append(d_read)

					if left_read == 0:
						# create binary data and return
						final_data = b"".join(bin_read_data)
						break
				

				return (read_len, final_data)


			def clear(self):
				pass

              
		return SharedMemStruct(mem_addr, mem_size)



	def get_username(self):
		return self._user


	def connect_to_service(self):
		"""
			Initialize the user and try to connect to the 
			sing storage service for data storage processing.
		"""

		tmp_logger = logging.getLogger(__name__)
		tmp_logger.info("connect_to_service")

		ctrl_sock = sing_ipc.ControlIPC.create_control_ipc(
					 sing_ipc.CONTROL_SOCKET)


		if not ctrl_sock:
			return sing_errs.INTERNAL_ERROR


		# try to connect to the sing service

		try:
			ctrl_sock.init_ipc()
			res_msg = ctrl_sock.connect_to_service( self._user,
					          	 					self._passwd)

			# two cases possible
	
			# case 1: MSG_STATUS returned

			if res_msg.msg_type == sing_msgs.MSG_STATUS:
				# get the return code
				res_service = res_msg.op_status

			elif res_msg.msg_type == sing_msgs.MSG_CON_REPLY:
				res_service = sing_errs.SUCCESS
			
			else: # some error
				res_service = sing_errs.INTERNAL_ERROR

		except:
			res_service = sing_errs.INTERNAL_ERROR

		# failed to connect to the storage service
		if res_service != sing_errs.SUCCESS:
			ctrl_sock.close_conn() # close the socket
			return res_service



		# Succesfully connected to the service
		# Allocate the communication structures
		try:
			# initialize the internal memory buffers

			handles = self._init_shared_memory(res_msg.write_buf_addr, 
									 res_msg.write_buf_size,
									 res_msg.write_buf_name, 
									 res_msg.read_buf_addr,
									 res_msg.read_buf_size,	
									 res_msg.read_buf_name)


			# create an IO handler
			self._io_handler = UserContext.__IOHandler__(ctrl_sock, 
												handles[0],
												handles[1])

		except Exception as exp:
			tmp_logger.error("UserCtx::connect_to_service: cannot init shared mem '{0}'".format(exp))
			res_service = sing_errs.INTERNAL_ERROR
	

		if res_service != sing_errs.SUCCESS:
			self.close()
			return res_service
			

		# all initialization steps have successfully completed
		# Notify the user about a successful process
		tmp_logger.info("Returning sing_errs.SUCCESS")
		return sing_errs.SUCCESS


	def _init_shared_memory(self, write_addr, write_size, write_name,
                                  read_addr,  read_size,  read_name):
		"""
			After a successful connection establishment with the
			sing local service, initialize the internal data structures.
		"""
		write_handle = self._create_write_buf(write_addr, write_size)
		read_handle  = self._create_read_buf(read_addr,  read_size)
        
        # check if the objects have been created properly
		if not write_handle or not read_handle:
			raise sing_errs.InternalError(sing_errs.INT_ERR_MEMORY)

        # try to connect to the memory regions
        # the system may throw some exceptions
		write_handle.init_mem(write_name, write_size)
		read_handle.init_mem(read_name, read_size)


		return (read_handle, write_handle)



	def get_user_property(self, prop_key):
		"""
			Get user property for data processing and encoding.
		"""
		return self._props.get_property(prop_key)


	def set_user_property(self, prop_key, prop_val):
		"""
			Set user internal properties for data processing.
		"""

		
		self._props.set_properties({prop_key : prop_val})


	def set_properties(self, prop_obj):
		"""
			Set user internal properties for data processing. 
		"""
		self._props = prop_obj
		

	def append_operation(self, op_ctx):
		
		tmp_logger = logging.getLogger(__name__)
		tmp_logger.info("UserCtx::append_operation")


		if self._io_handler and not self._io_handler.is_closed():
			self._io_handler.append_new_operation(op_ctx)

		else:
			# log this case as it is not supposed
			# to happen
			tmp_logger = logging.getLogger(__name__)
			tmp_logger.warn("UserCtx::append_operation: self._io_handler is None is closed.")
			op_ctx.set_result(sing_errs.AuthError())



	def close(self):
		"""
			Try to close the user.
		"""

		if self._io_handler:
			self._io_handler.close_handler()
			self._io_handler = None


	def is_connected(self):
		"""
			State of the user's control socket.
		"""
		return (self._io_handler is not None and\
					not self._io_handler.is_closed())


	def __bool__(self):
		return True

	__nonzero__ = __bool__
		
