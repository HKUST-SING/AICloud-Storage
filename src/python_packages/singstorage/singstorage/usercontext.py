# The file contains the user's context (its properties and io ops).
# The class given in the file encamsulates the state that the API has
# to maintain per active user.


# Dependency modules
from future.utils import viewitems as future_viewitems
import posix_ipc


# Python std lib
import os
import mmap
import logging


# singstorage modules
import singstorage.singexcept        as sing_errs
import singstorage.ipc               as sing_ipc
import singstorage.utils.hash        as sing_hash
import singstorage.messages          as sing_msgs
import singstorage.utils.loc_logging as sing_log



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
		requests to the sorage cluster. 
	"""
    
	def __init__(self, username, password):
		self._user        =    username
							   
                               # use only a hash of the password
		self._passwd      =    sing_hash.sha256_hash(password) 
		self._props       =    StorageProperties()
		self._ctrl        =    None  # IPC Control Channel

		self._id          =    0 # unique operation ID
                               # tuple: (path, left_to_read)
		self._pend_reads  =    [] # pending reads
                               # tupel: (path, left_to_write)
		self._pend_writes =    [] # pending writes  
		self._write_mem   =    None  # shared memory for writing data 
		self._read_mem    =    None  # shared memory for reading data



	def _create_write_buf(self, mem_addr, mem_size):

		class SharedMemStruct(object):
			def __init__(self, start_addr, region_size):
				self._beg_addr = start_addr # 
				self._mem_head = start_addr
				self._mem_tail = start_addr - 1 # have one byte deviation 
				self._end_addr = start_addr + region_size
				self._fd_shm   = None
				self._mmap     = None


			def init_mem(self, mem_name):
				self._fd_shm = posix_ipc.SharedMemory(
							   mem_name, 0, 0, 0, read_only=False)
				self._mmap   = mmap.mmap(self._fd_shm.fd, 0,
							   prot=mmap.PROT_READ | mmap.PROT_WRITE,
                               flags=mmap.MAP_SHARED)


			def close_mem(self):
				self._mmap.close()
				self._fd_shm.close_fd()
				self._mem_head = self._mem_tail =\
				self._beg_addr = self._end_addr = None


			def can_close_mem(self): # check before closing 
                                     # if all pending data has been
                                     # approved by the storage service
				if self._mem_head > self._mem_tail:
					return (self._mem_head == self._mem_tail) == 1
                
				return (self._mem_head == self._beg_addr) and\
					   (self._mem_tail == (self._end_addr - 1))
                    


			def write_data(self, data):
				# check if there is available data
				if self._mem_head == self._mem_tail:
					return 0 # cannot write (full window)

              
                
				# move the file descriptor to the correct position
				self._mmap.seek(self._mem_head - self._beg_addr, 
								os.SEEK_SET)

				# check how many bytes it can be written 
				need_to_write = len(data) # this many bytes 
										  # has to be written
                
				avail = (self._end_addr - self._mem_head)\
						if(self._mem_head > self._mem_tail)\
						else (self._mem_tail - self._mem_head)

				# check how many bytes can be written in one call
				will_write = min(avail, need_to_write)

				# compute # written bytes
				bytes_written = self._mmap.tell()
				self._mmap.write(bytes(data[0:will_write:1]))
				bytes_written = (self._mmap.tell() - bytes_written)

				# update the head pointer to the new position
				self._mem_head = (self._mem_head + bytes_written) % self._end_addr

				return bytes_written
                

            
			def avail_buffer(self):
				if self._mem_head > self._mem_tail:
					return (self._end_addr - self._mem_head) +\
						   (self._mem_tail - self._beg_addr)
				else:
					return (self._mem_tail - self._mem_head)



			def get_write_addr(self):
				return self._mem_head # return the memory address for
                                      # writing 


			def release_mem(self, buf_size): # data has been read by
                                             # the control service
				self._mem_tail =\
					  (self._mem_tail + buf_size) % self._end_addr


		return SharedMemStruct(mem_addr, mem_size)



	def _create_read_buf(self, mem_addr, mem_size):

		class SharedMemStruct(object):
			def __init__(self, start_addr, region_size):
				self._beg_addr = start_addr # 
				self._end_addr = start_addr + region_size
				self._fd_shm   = None
				self._mmap     = None


			def init_mem(self, mem_name):
				self._fd_shm = posix_ipc.SharedMemory(
							   mem_name, 0, 0, 0, read_only=True)
				self._mmap   = mmap.mmap(self._fd_shm.fd, 0,
                               prot=mmap.PROT_READ,
                               flags=mmap.MAP_SHARED)


			def close_mem(self):
				self._mmap.close()
				self._fd_shm.close_fd()
				self._beg_addr = self._end_addr = None

                    

			def read_data(self, read_addr, read_len):
				# if the read address is out of the range
				# throw an exception
				tmp_logger = logging.getLogger()
				tmp_logger.info("read_shared_memory")

				if  read_addr >= self._end_addr or\
					read_addr < self._beg_addr:

					tmp_logger.error("Reading address ranges are wrong")
					raise sing_errs.InternalError(sing_errs.INT_ERR_PROT)

				# read the given number of bytes and 
				# return a tuple: (read_bytes, data_string)
                # move to the correct place of the file
				self._mmap.seek((read_addr - self._beg_addr), os.SEEK_SET)
                
				can_read = min(read_len, self._end_addr - read_addr)
				d_read = self._mmap.read(can_read)

				return (len(d_read), d_read)

              
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

		self._ctrl = sing_ipc.ControlIPC.create_control_ipc(
					 sing_ipc.CONTROL_SOCKET)


		if not self._ctrl:
			return sing_errs.INTERNAL_ERROR


		# try to connect to the sing service

		try:
			self._ctrl.init_ipc()
			res_msg = self._ctrl.connect_to_service(self._user,
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
			self._ctrl.close_conn()
			res_service = sing_errs.INTERNAL_ERROR

		# failed to connect to the storage service
		if res_service != sing_errs.SUCCESS:
			return res_service



		# Succesfully connected to the service
		# Allocate the communication structures
		try:
			# initialize the internal memory buffers

			self._init_shared_memory(res_msg.write_buf_addr, 
									 res_msg.write_buf_size,
									 res_msg.write_buf_name, 
									 res_msg.read_buf_addr,
									 res_msg.read_buf_size,	
									 res_msg.read_buf_name)

		except Exception as exp:
			print (exp)
			self.close()
			return sing_errs.INTERNAL_ERROR # add logging here
			

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
		self._write_mem = self._create_write_buf(write_addr, write_size)
		self._read_mem  = self._create_read_buf(read_addr,  read_size)
        
        # check if the objects have been created properly
		if not self._write_mem or not self._read_mem:
			raise sing_errs.InternalError(sing_errs.INT_ERR_MEMORY)

        # try to connect to the memory regions
        # the system may throw some exceptions
		self._write_mem.init_mem(write_name)
		self._read_mem.init_mem(read_name)



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


	def _get_unique_id(self):
		"""
			A way of assigning the id to a unique value
		"""
		
		# first, only a simple increment
		uq_id = (self._id +1) % sing_msgs.MAX_SEQ_ID

		return uq_id



	def write_raw_data(self, rados_obj):
		"""
			Write rados object to the shared memory
		"""
	
		tmp_logger = logging.getLogger(__name__)
		tmp_logger.info("write_raw_data")
	
		# rados object has a length so that it could
		# keep writing until fully written
		ref_data = rados_obj.get_raw_data() # reference to data
		obj_len  = rados_obj.get_len()      # length in bytes
	

		# request for a permission from the service
		# to write the new data
		mark_id = self._get_unique_id() # message id
		self._ctrl.send_request(sing_msgs.MSG_WRITE, mark_id,
								data_path=rados_obj.get_data_path(),
								properties=1, start_addr=0,
								data_length=obj_len)


		# wait for permission from the service
		res = self._ctrl.recv_request(sing_msgs.MSG_READ)
		# check if the message is either a READ or a STATUS 
		# message
		# some error occured
		if res.msg_type == sing_msgs.MSG_STATUS:
			# throw an exception
			if res.op_status == sing_msgs.ERR_PATH:
				raise sing_errs.PathError(rados_obj.get_data_path(),
										  False)

			elif res.op_status == sing_msgs.ERR_DENY:
				raise sing_errs.PathError(rados_obj.get_data_path(),
										  True)

			elif res.op_status == sing_msgs.ERR_QUOTA:
				raise sing_errs.QuotaError(obj_len, 0)

			elif res.op_status == sing_msgs.ERR_PROT:
				protocol = rados_obj.get_data_path().split(":")
				raise sing_errs.ProtError(protocol[0])

			else: # some internal error
				raise sing_errs.InternalError(sing_errs.INT_ERR_UNKNOWN)


		# make sure a READ response
		assert res.msg_type == sing_msgs.MSG_READ, "Not READ msg"
	
		# since it is the first write message, expect to
		# receive a read with the bitmap flag set
		assert res.prop_bitmap == 1, "READ bitmap unset"


	
		# keep writing until an exception occurs or
		# the entire object has been written
		mem_addr = self._write_mem.get_write_addr() # where data 
												    # will be written	

		written_bytes = self._write_mem.write_data(ref_data)


		if written_bytes <= 0: # something wrong
			tmp_logger.error("'written_bytes' <= 0")
			self.close()
			raise sing_errs.InternalError(sing_errs.INT_ERR_WRITE)


		obj_len -= written_bytes # update the number of written bytes
		
		# send a notification to the service about the data
		mark_id = self._get_unique_id()
		self._ctrl.send_request(sing_msgs.MSG_WRITE, mark_id,
								data_path=rados_obj.get_data_path(),
								properties=0, start_addr=mem_addr,
								data_length=written_bytes)


		# wait for response 
		res = self._ctrl.recv_request(sing_msgs.MSG_READ)
		# check if the message is either a READ or a STATUS 
		# message
		# some error occured
		if res.msg_type == sing_msgs.MSG_STATUS:
			# throw an exception
			# some internal error
			raise sing_errs.InternalError(sing_errs.INT_ERR_UNKNOWN)


		# make sure a READ response
		assert res.msg_type == sing_msgs.MSG_READ, "Not READ msg"
		# the flag must be unset
		assert res.prop_bitmap == 0, "READ bitmap set"


		# write the rest of the data
		std_index = written_bytes

		while obj_len > 0: # write all the data
			mem_addr = self._write_mem.get_write_addr()
			send_buf = self._avail_buffer()
			written_bytes = self._write_mem.write_data(
					ref_data[std_index:std_index+send_buf:1])


			# check the number of bytes
			if written_bytes <= 0:
				self.close()
				raise sing_errs.InternalError(sing_errs.INT_ERR_WRITE)

			# send a requst to the service
			mark_id = self._get_unique_id()
			self._ctrl.send_request(sing_msgs.MSG_WRITE, mark_id,
									data_path=rados_obj.get_data_path(), 
									properties=0,
                                    start_addr=mem_addr, 
									data_length=written_bytes)

			# update the values
			obj_len -= written_bytes
			std_index += written_bytes

			# wait for a response from the service
			res = self._ctrl.recv_request(sing_msgs.MSG_READ)
			# check the status of the previous write

			if res.msg_type == sing_msgs.MSG_STATUS:
				# some internal error
				raise sing_errs.InternalError(sing_errs.INT_ERR_UNKNOWN)


			assert res.msg_type == sing_msgs.MSG_READ, "Not READ msg"

			# ensure that the bit flag is reset
			assert res.prop_bitmap == 0, "READ bitmap is set"
	
	

		# done with the rados object
		mark_id = self._get_unique_id()	
		self._ctrl.send_request(sing_msgs.MSG_WRITE, mark_id,
								data_path=rados_obj.get_data_path(), 
								properties=0,
                                start_addr=0, 
							    data_length=0)


		res = self._ctrl.recv_request(sing_msgs.MSG_READ)
		# check the status of the previous write
		assert res.msg_type == sing_msgs.MSG_READ, "Not READ msg"

		# ensure that the bit flag is reset
		assert res.prop_bitmap == 0, "READ bitmap is set"



	def read_raw_data(self, rados_obj):
		"""
			Read a rados object from the shared memory.
		"""

		tmp_logger = logging.getLogger(__name__)
		tmp_logger.info("read_raw_data")
		

		ref_data = rados_obj.get_raw_data() # reference to
											# raw data of the object
		
		# ask for request from the service
		mark_id = self._get_unique_id()
		self._ctrl.send_request(sing_msgs.MSG_READ, mark_id,
							    data_path=rados_obj.get_data_path(),
								properties=1)
	

		# wait for response 
		res = self._ctrl.recv_request(sing_msgs.MSG_WRITE)

		# check the status of the previous message
		if res.msg_type == sing_msgs.MSG_STATUS:
			tmp_logger.error("read_raw_data: == MSG_STATUS")
			
			if res.op_status == sing_msgs.ERR_PATH:
				raise sing_errs.PathError(rado_obj.get_data_path(),
                                          False)
		
			elif res.op_status == sing_msgs.ERR_DENY:
				raise sing_errs.PathError(rados_obj.get_data_path(),
										  True)

			else:
				tmp_logger.warn("MSG_STATUS: non path related read")
				raise sing_errs.InternalError(sing_errs.INT_ERR_UNKNOWN)


		if res.data_path != rados_obj.get_data_path(): # some internal
													   # error
			tmp_logger.error("read_raw_data: data_path != rados.data_path")
			self.close()
			raise sing_errs.InternalError(sing_errs.INT_ERR_PROT) 


		assert res.msg_id == mark_id, "Not the same ID"
		#assert res.prop_bitmap == 1,  "Write flag is unset"


		# read data from the shared memory
		read_bytes, read_data = self._read_mem.read_data(res.mem_addr,
														 res.data_length)

		if read_bytes != res.data_length:
			# an error, notify the service
			self._ctrl.send_request(sing_msgs.MSG_STATUS, mark_id,
									status_type=sing_msgs.ERR_INTER)

			tmp_logger.error("read_raw_data: read_bytes != res.data_length")
			self.close()

			raise sing_errs.InternalError(sing_errs.INT_ERR_READ)
		else:
			# notify that the data has been read
			self._ctrl.send_request(sing_msgs.MSG_READ, mark_id,
							    	data_path=rados_obj.get_data_path(),
									properties=0)

		# append data
		rados_obj.extend_data(read_data)
		# wait for the next message from the service
		while True:
		
			# next chunk of data
			res = self._ctrl.recv_request(sing_msgs.MSG_WRITE)	

			assert res.msg_type == sing_msgs.MSG_WRITE, "Not a WRITE msg"

			# check if writing is complete
			if res.mem_addr == 0 and res.data_length == 0:
				# notify a success
				self._ctrl.send_request(sing_msgs.MSG_READ, 
									    mark_id,
									    data_path=rados_obj.get_data_path())
				
				return # done reading the rados object

 
			assert res.msg_id == mark_id, "Not the same ID as read's one"


			# read data from the shared memory
			read_bytes, read_data = self._read_mem.read_data(res.mem_addr,
														 res.data_length)

			if read_bytes != res.data_length:

				# an error, notify the service
				self._ctrl.send_request(sing_msgs.MSG_STATUS, mark_id, 
									status_type=sing_msgs.ERR_INTER)

				tmp_logger.error("read_raw_data: read_bytes != res.data_length")
				self.close()
				raise sing_errs.InternalError(sing_errs.INT_ERR_PROT)

			else:
				# notify a success
				self._ctrl.send_request(sing_msgs.MSG_READ, mark_id, 
									data_path=rados_obj.get_data_path())


			# append data and loop back
			rados_obj.extend_data(read_data)
			

	def delete_data(self, rados_obj):
		"""
			Delete a storage object at the given path.
		"""
		tmp_logger = logging.getLogger(__name__)
		tmp_logger.info("delete_data: {0}".format(rados_obj.get_data_path()))		

		mark_id = self._get_unique_id() # message id
		self._ctrl.send_request(sing_msgs.MSG_DELETE, mark_id,
								data_path=rados_obj.get_data_path())


		# wait for permission from the service
		res = self._ctrl.recv_request(sing_msgs.MSG_STATUS) 
		
		# STATUS message must be received
		assert res.msg_type == sing_msgs.MSG_STATUS, "Delete receives some different message"
		assert res.msg_id == mark_id, "STATUS id != message id"


		# if status is success, return
		if res.op_status == sing_msg.STAT_SUCCESS:
			return	


		# other possible cases
		if res.op_status == sing_msgs.ERR_PATH:
			raise sing_errs.PathError(rados_obj.get_data_path(),
									  False)

		elif res.op_status == sing_msgs.ERR_DENY:
			raise sing_errs.PathError(rados_obj.get_data_path(),
									  True)

		elif res.op_status == sing_msgs.ERR_QUOTA:
			raise sing_errs.QuotaError(obj_len, 0)

		elif res.op_status == sing_msgs.ERR_PROT:
			protocol = rados_obj.get_data_path().split(":")
			raise sing_errs.ProtError(protocol[0])

		else: # some internal error
			raise sing_errs.InternalError(sing_errs.INT_ERR_UNKNOWN)



	def close(self):
		"""
			Try to close the user.
		"""

		tmp_logger = logging.getLogger(__name__)
		tmp_logger.info("close")
		
		# first release the read/write buffers
		try:
			self._read_mem.close_mem()
		except:
			tmp_logger.error("close: self._read_mem.close_mem()")

		try:
			self._write_mem.close_mem()
		except:
			tmp_logger.error("close: self._write_mem.close_mem()")

		# close the control communication channel
		try:
			self._ctrl.close_conn()

		except:
			tmp_logger.error("close: self._ctrl.close_conn()")


	def is_connected(self):
		"""
			State of the user's control socket.
		"""
		return self._ctrl.is_connected()
		
