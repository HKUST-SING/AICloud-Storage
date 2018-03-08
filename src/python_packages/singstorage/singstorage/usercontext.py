# The file contains the user's context (its properties and io ops).
# The class given in the file encamsulates the state that the API has
# to maintain per active user.


# Dependency modules
from __future__ import print_function
from future.utils import viewitems as future_viewitems



# Python std lib



# singstorage modules
import singstorage.singexcept as sing_errors
import singstorage.ipc        as sing_ipc
import singstorage.utils.hash as sing_hash
import singstorage.messages   as sing_msgs



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
			raise sing_errors.PropertyException(
				"Property \"{0}\" does not exist".format(key))

		return True



	def _check_val(self, prop_key, prop_val):
		"""
			Check if the property can be set to the pro_val
		"""
		# get property options
		options = StorageProperties.__PROPERTIES__.get(prop_key)
		if not options.get(prop_val, False): # cannot set to the value
			raise sing_errors.PropertyException(
				"Property \"{0}\" cannot be set to \"{1}\".\nAvailable values: {2}".format(prop_key, prop_val, options))

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
				self._mmap.seek(self._mem_head - self._mem_beg, 
								os.SEEK_SET)

				# check how many bytes it can be written 
				need_to_write = len(data) # this many bytes 
										  # has to be written
                
				avail = (self._end_addr - self._mem_head)\
						if(self._mem_head > self._mem_tail)\
						else (self._mem_tail - self._mem_head)

				# check how many bytes can be written in one call
				will_write = min(avail, need_to_write)
				bytes_written = self._mmap.write(data[0:will_write:1])

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


		return SharedMemeStruct(mem_addr, mem_size)



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
				if  read_addr >= self._end_addr or\
					read_addr < self._beg_addr:
					raise BufferError("Read Address out of Range")

				# read the given number of bytes and 
				# return a tuple: (read_bytes, data_string)
                # move to the correct place of the file
				self._mmap((read_addr - self._beg_addr), os.SEEK_SET)
                
				can_read = min(read_len, self._end_addr - read_addr)
				d_read = self._mmap.read(can_read)

				return (len(d_read), d_read)

              
		return SharedMemeStruct(mem_addr, mem_size)



	def get_username(self):
		return self._user


	def connect_to_service(self):
		"""
			Initialize the user and try to connect to the 
			sing storage service for data storage processing.
		"""
		return
		self._ctrl = ControlIPC.create_control_ipc(
					 sing_ipc.CONTROL_SOCKET)


		if not self._ctrl:
			raise RuntimeError("Out of memory")


		# try to connect to the sing service
		try:
			self._ctrl.init_ipc()
			self._ctrl.connect_to_service(self._user,
										  self._passwd)

		except SingIOError as exp: # some specific error
			return exp.err_code

		except:                    # system internal error 
			return sing_errors.INTERNAL_ERROR


		# succesfully connected to the service
		# wait for the response from the service
		# and initialize internal data buffers.
		try:
			req = self._ctrl.recv_request(sing_msgs.MSG_CON_REPLY)

			# initialize the internal memory buffers
			self._init_shared_memory(req.write_buf_addr, 
									 req.write_buf_size,
									 req.write_buf_name, 
									 req.read_buf_addr,
									 req.read_buf_size,	
									 req.read_buf_name)

		except Exception as exp:
			return sing_errors._INTERNAL_ERROR # add logging here
			


		# all initialization steps have successfully completed
		# return success
		return sing_errors.SUCCESS





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
			raise SingIOError("System is out of memory")

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





	def write_raw_data(self, rados_obj):
		"""
			Write rados object to the shared memory
		"""
		
		# rados object has a length so that it could
		# keep writing until fully written
		ref_data = rados_obj.get_raw_data() # reference to data
		obj_len  = rados_obj.get_len()      # length in bytes
		
		return
		# keep writing until an exception occurs or
		# the entire object has been written
		mem_addr = self._write_mem.get_write_addr() # where data 
												    # will be written	

		written_bytes = self._write_mem(ref_data)


		if written_bytes <= 0: # something wrong
			raise SingIOError("Error: send_request", 
							   sing_errors.INTERNAL_ERROR)


		obj_len -= written_bytes # update the number of written bytes
		
		# send a notification to the service about the data
		self._ctrl.send_request(sing_msgs.MSG_WRITE,
								data_path=rados_obj.get_data_path(),
								properties=0, start_addr=mem_addr,
								data_length=written_bytes)


		# wait for response 
		res = self._ctrl.recv_request(sing_msgs.MSG_STATUS)
		# check the status of the previous message
		if res.op_status != sing_msgs.STAT_SUCCESS:
			raise SingIOError("Error: recv_request", res.op_status)

		
		# write the rest of the data
		std_index = written_bytes

		while obj_len > 0:
			mem_addr = self._write_mem.get_write_addr()
			send_buf = self._avail_buffer()
			written_bytes = self._write_mem(
					ref_data[std_index:std_index+send_buf:1])


			# check the number of bytes
			if written_bytes <= 0:
				raise SingIOError("Error: send_request", 
								   sing_errors.INTERNAL_ERROR)

			# send a requst to the service
			self._ctrl.send_request(sing_msgs.MSG_WRITE,
									data_path="", properties=0,
                                    start_addr=mem_addr, 
									data_length=written_bytes)

			# update the values
			obj_len -= written_bytes
			std_index += written_bytes

			# wait for a response from the service
			res = self._ctrl.recv_request(sing_msgs.MSG_STATUS)
			# check the status of the previous message
			if res.op_status != sing_msgs.STAT_SUCCESS:
				raise SingIOError("Error: recv_request", res.op_status)
	
	

		# done with the rados object	




	def read_raw_data(self, rados_obj):
		"""
			Read a rados object from the shared memory.
		"""
	
		return	
		ref_data = rados_obj.get_raw_data() # reference to
											# raw data of the object
		
		# ask for request from the service
		self._ctrl.send_request(sing_msgs.MSG_READ, 
							    data_path=rados_obj.get_data_path())
	

		# wait for response 
		res = self._ctrl.recv_request(sing_msgs.MSG_STATUS)
		# check the status of the previous message
		if res.op_status != sing_msgs.STAT_SUCCESS:
			raise SingIOError("Error: recv_request", res.op_status)

		
		# wait for the first chunk of data
		res = self._ctrl.recv_request(sing_msgs.MSG_WRITE)

		if res.data_path != rados_obj.get_data_path(): # some internal
													   # error
			raise SingIOError("Retrieved wrong data from storage.", 
							  sing_errors.INTERNAL_ERROR) 


		# read data from the shared memory
		read_bytes, read_data = self._read_mem.read_data(res.mem_addr,
														 res.data_length)

		if read_bytes != res.data_length:
			# an error, notify the service
			self._ctrl.send_request(sing_msgs.MSG_STATUS, 
									status_type=sing_errors.INTERNAL_ERROR)
			raise SingIOError("Cannot retrieve data", 
							  sing_errors.INTERNAL_ERROR)
		else:
			# notify a success
			self._ctrl.send_request(sing_msgs.MSG_STATUS, 
									status_type=sing_error.SUCCESS)


		# append data
		rados_obj.extend_data(read_data)
		# wait for the next message from the service
		while True:
		
			# next chunk of data
			res = self._ctrl.recv_request(sing_msgs.MSG_WRITE)

			# check if writing is complete
			if res.data_path == "" and res.data_length == 0:
				# notify a success
				self._ctrl.send_request(sing_msgs.MSG_STATUS, 
									status_type=
									sing_msgs.STAT_SUCCESS)
				
				return # done reading the rados object

 

			if res.data_path != rados_obj.get_data_path(): # some internal
										      			   # error
				raise SingIOError("Retrieved wrong data from storage.", 
							  	   sing_errors.INTERNAL_ERROR) 


			# read data from the shared memory
			read_bytes, read_data = self._read_mem.read_data(res.mem_addr,
														 res.data_length)

			if read_bytes != res.data_length:

				# an error, notify the service
				self._ctrl.send_request(sing_msgs.MSG_STATUS, 
									status_type=sing_errors.INTERNAL_ERROR)
				raise SingIOError("Cannot retrieve data", 
							  sing_errors.INTERNAL_ERROR)
			else:
				# notify a success
				self._ctrl.send_request(sing_msgs.MSG_STATUS, 
									status_type=sing_error.SUCCESS)


			# append data and loop back
			rados_obj.extend_data(read_data)
			


	def  close(self):
		"""
			Try to close the user.
		"""
		return
		
		# first release the read/write buffers
		try:
			self._read_mem.close_mem()
		except:
			pass # log this

		try:
			self._write_mem.close_mem()
		except:
			pass # log this

		# close the control communication channel
		try:
			self._ctrl.close_conn()

		except:
			pass # log this 


	def is_connected(self):
		"""
			State of the user's control socket.
		"""
		return self._ctrl.is_connected()
		
