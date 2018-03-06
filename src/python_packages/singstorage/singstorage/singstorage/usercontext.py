# The file contains the user's context (its properties and io ops).
# The class given in the file encamsulates the state that the API has
# to maintain per active user.


# Dependency modules
from future.utils import viewitems as future_viewitems



# Python std modules






# Package modules
import singstorage.singexcept as errors




class UserProperties(object):
	"""
		Class is a container for data processing properties.
		Properties are metadata for the stored data. For example,
		encoding, data compression, and so on.
	"""

	__DATA_PROPERTIES__ = {"encoding" : {"utf-8":True}}
 

	def __init__(self):
		self._vals = {"encoding" : "utf-8"}


	def _check_key(self, prop_key):
		"""
			Check if the property exists. If does not exist, throw
			an exception.
		"""
        
		res = UserProperties.__DATA_PROPERTIES__.get(prop_key, None)
		if not res: # no such propery
			raise errors.PropertyException(
				"Property \"{0}\" does not exist".format(key))

		return True



	def _check_val(self, prop_key, prop_val):
		"""
			Check if the property can be set to the pro_val
		"""
		# get property options
		options = UserProperties.__DATA_PROPERTIES__.get(prop_key)
		if not options.get(prop_val, False): # cannot set to the value
			raise errors.PropertyException(
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
		for prop_key, prop_val in future_viewitems(kwargs):
			self._check_key(prop_key)
			self._check_value(prop_val)
			# can set the property
			self._props[prop_key] = prop_val

        


class UserCtx(object):
	""" 
		Stores user context for smooth communication between the 
		Python package and the system serivce that serves data
		requests to the sorage cluster. 
	"""
    
	def __init__(self, username, password):
		self._user        =    username
		self._passwd      =    password
		self._props       =    UserProperties()
		self._connected   =    False # if user connected
		self._socket      =    None  # UNIX socket for cotrol msgs

                               # tuple: (path, left_to_read)
		self._pend_reads  =    [] # pending reads
                               # tupel: (path, left_to_write)
		self._pend_writes =    [] # pending writes  
		self._send_mem    =    None  # memory pointer of send buffer 
		self._recv_mem    =    None  # memory pointer of recv buffer



	def _create_send_buf(self, mem_addr, mem_size):

		class SharedMemStruct(object):
			def __init__(self, start_addr, region_size):
				self._beg_addr = start_addr # 
				self._mem_head = start_addr
				self._mem_tail = start_addr - 1 # have one byte deviation 
				self._end_addr = start_addr + region_size
				self._fd_shm   = None
				self._mmap     = None


			def init_buff(self, mem_name):
				self._fd_shm = posix_ipc.SharedMemory(
							   mem_name, 0, 0, 0, read_only=False)
				self._mmap   = mmap.mmap(self._fd_shm.fd, 0,
							   prot=mmap.PROT_READ | mmap.PROT_WRITE,
                               flags=mmap.MAP_SHARED)


			def close_buff(self):
				self._mmap.close()
				self._fd_shm.close_fd()
				self._mem_head = self._mem_tail =\
				self._beg_addr = self._end_addr = None


			def can_close_buf(self): # check before closing 
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
				bytes_written = min(avail, need_to_write)
				self._mmap.write(data[0:bytes_written:1])

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


			def init_buff(self, mem_name):
				self._fd_shm = posix_ipc.SharedMemory(
							   mem_name, 0, 0, 0, read_only=True)
				self._mmap   = mmap.mmap(self._fd_shm.fd, 0,
                               prot=mmap.PROT_READ,
                               flags=mmap.MAP_SHARED)


			def close_buff(self):
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

	def _init_data_buffers(self, write_addr, write_size, write_name,
                                 read_addr,  read_size,  read_name):
		"""
			After a successful connection establishment with the
			sing local service, initialize the internal data structures.
		"""
		self._send_mem = self._create_send_buf(write_addr, write_size)
		self._recv_mem = self._create_recv_buf(read_addr,  read_size)
        
        # check if the objects have been created properly
		if not self._send_mem or not self._rec_mem:
			raise SingIOError("System is out of memory")

        # try to connect to the memory regions
        # the system may throw some exceptions
		self._send_mem.init_buf(write_name)
		self._rec_mem.init_buf(read_name)

	def get_user_property(self, prop_key):
		"""
			Get user property for data processing and encoding.
		"""
		return self._props.get_property(prop_key)


	def set_user_properties(self, **kwargs):
		"""
			Set user internal properties for data processing.
		"""
		self._props.set_properties(**kwargs)


	def  close(self):
		"""
			Try to close the user.
		"""
		# If there are no 

		self._connected = False # not connected anymore
