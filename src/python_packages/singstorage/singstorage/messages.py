# File contains inter-process communication messages described
# in the 'Inter-Processing Message Structrute' document on github.com


# Dependency packages

# Python std
import struct


# singstorage packages 
from singstorage.utils import encoding
from singstorage       import PYTHON_MAJOR_VERSION


# MESSAGE TYPE VALUES
MSG_DEFAULT    =  255
MSG_STATUS     =    0
MSG_AUTH       =    1
MSG_READ       =    2
MSG_WRITE      =    3
MSG_CON_REPLY  =    4
MSG_CLOSE      =    5
MSG_DELETE     =    6
MSG_RELEASE    =    7


# 'status_type' for MSG_STATUS messages
# keep it consistent with the client library
# ("include/CommonCode.h")
STAT_SUCCESS    = 0   # notifies success
ERR_AUTH_USER   = 1   # authentication problem (username)
ERR_AUTH_PASS   = 2   # authentication problem (password)
ERR_PATH	    = 3   # data path problem ('no such path')
ERR_DENY	    = 4   # denied access to the data at 'path'
ERR_QUOTA       = 5   # user has exceeded his/her data quota
ERR_DATA_LARGE  = 6   # data too big to write at once
ERR_DATA_SMALL  = 7   # data to small to write at once (buffer)
ERR_CONTENT     = 8   # message content cannot be understood
ERR_PARAMS      = 9   # parameters cannot be understood
ERR_PROT        = 10  # the used protocol is not supported
ERR_LOCK        = 11  # cannot acquire lock on 'path'
STAT_PART_READ  = 12  # there is more data to read
STAT_PART_WRITE = 13  # there is a merging write 
                       # (few writes merged into one)
STAT_CLOSE      = 14  # require to close IPC and release resources


ERR_AMBG        = 254 # cannot understand previously sent request
ERR_INTER       = 255 # internal system error (release resources)



MAX_SEQ_ID  = 2**32 - 1 # uint32_t max value
HASH_LENGTH = 32
HASH_CODE = "B"*HASH_LENGTH # hash string value
class InterMessage(object):
	"""
		Inter-process communication message base.
	"""
	subcls = {} # list of subclasses : key is class 

	def __init__(self, msg_type=MSG_DEFAULT, 
					   msg_length=0, msg_id=0):
		self.msg_type   = msg_type
		self.msg_id     = msg_id
		self.msg_length = msg_length + 9


	def encode_msg(self):
		"""
			Write the style, the type, and length.
		"""
		return ("=BII", [self.msg_type, self.msg_id, self.msg_length])


	def decode_msg(self, message):
		self.decode_header()


	def decode_header(self, header_msg):
		self.msg_type, self.msg_id, self.msg_length =\
		  struct.unpack("=BII", header_msg[0:9:1])



	def get_header_size(self):
		return 9


	def get_msg_length(self):
		return self.msg_length


	def get_msg_type(self):
		return self.msg_type

	def get_msg_id(self):
		return self.msg_id


	@classmethod
	def register_subclass(cls, msg_type):
		"""
			Class decorator for subclasses in order to register
			with the superclass.
		"""
		def add_sub(subclass):
			cls.subcls[msg_type] = subclass

			return subclass

		return add_sub # register all subclasses



	@classmethod
	def create_message(cls, msg_type, msg_id, **kwargs):
		"""
			Create and initialize a new message of type 'msg_type'
		"""
		MessageClass = cls.subcls.get(msg_type, None)
			
		if not MessageClass:
			return None

		# create a message with the passed values
		msg = MessageClass(**kwargs)
		# modify the id 
	
		msg.msg_id = msg_id

		return msg


@InterMessage.register_subclass(MSG_AUTH)
class AuthMessage(InterMessage):
	"""
		Authentication message.
	"""
	def __init__(self, username="", passwd=b""):
		super(AuthMessage, self).__init__(MSG_AUTH, 
										  len(username)+HASH_LENGTH + 2)
		self.user_name    = username
		self.passwd_hash  = passwd 



	def encode_msg(self):
		"""
			Encode the content of the message into binary form.
		"""
		msg_beg, vals = super(AuthMessage, self).encode_msg()

		string_val = "B"*len(self.user_name)
		code_val = "".join([msg_beg, "H", string_val, HASH_CODE])
		vals.append(len(self.user_name)) # append length value
		
		# encode chars
		for char_name in self.user_name:
			vals.append(ord(char_name))
		
		
		for char_pass in self.passwd_hash[0:HASH_LENGTH:1]:
			if PYTHON_MAJOR_VERSION == 2:
				vals.append(ord(char_pass))

			elif PYTHON_MAJOR_VERSION == 3:
				vals.append(char_pass)

			else:
				pass # add logging here

		# done
		return struct.pack(code_val, *vals)



	
	def decode_msg(self, message):
		"""
			Decode the passsed binary message into the fields
			of the message.
		"""
		user_length = struct.unpack("=H", message[0:2:1])
		user_length = user_length[0] # from tuple to int
		chars = "B"*(user_length+HASH_LENGTH)
		# now unpack the rest of the chars
		name_pass = struct.unpack("="+chars, message[2::1])
		
		# decode interger values to strings
		self.user_name   =\
			"".join([str(chr(ch_item))\
			for ch_item in name_pass[0:user_length:1]])

		
		# decode the string of bytes
		if PYTHON_MAJOR_VERSION == 3:
			self.passwd_hash = bytes(name_pass[user_length::1])
		
		else: # must be Python2.7
			self.passwd_hash =\
			"".join([chr(ch_item) for ch_item
								  in name_pass[user_length::1]])
			
		
		

@InterMessage.register_subclass(MSG_READ)
class ReadMessage(InterMessage):
	"""
		Read request message.
	"""
	def __init__(self, data_path="", properties=0):
		super(ReadMessage, self).__init__(MSG_READ, 
										  len(data_path) + 6)
		self.data_path    = data_path
		self.prop_bitmap  = properties


	def encode_msg(self):
		"""
			Encode the content of the message into binary form.
		"""
		msg_beg, vals = super(ReadMessage, self).encode_msg()
		string_val = "B"*len(self.data_path)
		code_val = "".join([msg_beg, "H", string_val, "I"])
		vals.append(len(self.data_path)) # append length value
		
		# encode chars
		for char_name in self.data_path:
			vals.append(ord(char_name))
		
		# append property bitmap
		vals.append(self.prop_bitmap)

		# done
		return struct.pack(code_val, *vals)



	
	def decode_msg(self, message):
		"""
			Decode the passsed binary message into the fields
			of the message.
		"""
		path_length = struct.unpack("=H", message[0:2:1])
		path_length = path_length[0] # tuple to int
		chars = "B"*path_length
		# now unpack the rest of the chars
		vals  = struct.unpack("="+chars, message[2:2+path_length:1])
		
		# decode a binary string to a Python string
		self.data_path =\
		"".join([str(chr(ch_item)) for ch_item in vals[0::1]])

		# decode the property bitmap
		tmp_bit  = struct.unpack("=I", 
						message[2+path_length:6+path_length:1])
		self.prop_bitmap = tmp_bit[0] # tuple to int
		
	
@InterMessage.register_subclass(MSG_WRITE)
class WriteMessage(InterMessage):
	"""
		Write request message.
	"""
	def __init__(self,  data_path="", properties=0, 
				 start_addr=0, data_length=0):
		super(WriteMessage, self).__init__(MSG_WRITE, 
										  len(data_path) + 22)
		self.data_path    =  data_path
		self.prop_bitmap  =  properties
		self.mem_addr     =  start_addr
		self.data_length  =  data_length    


	def encode_msg(self):
		"""
			Encode the content of the message into binary form.
		"""
		msg_beg, vals = super(WriteMessage, self).encode_msg()
		string_val = "B"*len(self.data_path)
		code_val = "".join([msg_beg, "H", string_val, "IQQ"])
		vals.append(len(self.data_path)) # append lenght value
		
		# encode chars
		for char_name in self.data_path:
			vals.append(ord(char_name))
	
		# append property bitmap, memory address and data length	
		vals.append(self.prop_bitmap)
		vals.append(self.mem_addr)
		vals.append(self.data_length)


		# done
		return struct.pack(code_val, *vals)



	
	def decode_msg(self, message):
		"""
			Decode the passsed binary message into the fields
			of the message.
		"""
		path_length = struct.unpack("=H", message[0:2:1])
		path_length = path_length[0] # tuple to int
		chars = "B"*path_length
		# now unpack the rest of the chars
		path_data = struct.unpack("="+chars, message[2:path_length+2:1])


		# decode a binary string to a Python string
		self.data_path =\
		"".join([str(chr(ch_item)) for ch_item in path_data])
	

		
		# decode the rest of the message
		self.prop_bitmap, self.mem_addr, self.data_length =\
			struct.unpack("=IQQ", message[2+path_length:22+path_length:1])
		
		
	

@InterMessage.register_subclass(MSG_STATUS)
class StatusMessage(InterMessage):
	"""
		Operation status message.
	"""
	def __init__(self, status_type=255):
		super(StatusMessage, self).__init__(MSG_STATUS, 
										  2)
		self.op_status    = status_type
		    

	def encode_msg(self):
		"""
			Encode the content of the message into binary form.
		"""
		msg_beg, vals = super(StatusMessage, self).encode_msg()
		code_val =  "".join([msg_beg, "H"])
		vals.append(self.op_status)

		# done
		return struct.pack(code_val, *vals)



	
	def decode_msg(self, message):
		"""
			Decode the passsed binary message into the fields
			of the message.
		"""
		tmp_status     =  struct.unpack("=H", message[0:2:1])
		self.op_status =  tmp_status[0] # tuple to int
	
	
@InterMessage.register_subclass(MSG_CON_REPLY)
class ConReplyMessage(InterMessage):
	"""
		Connection request  message.
	"""
	def __init__(self, w_buf_addr=0,  w_buf_size=0,
				       r_buf_addr=0,  r_buf_size=0,
                       w_buf_name="", r_buf_name=""):
		super(ConReplyMessage, self).__init__(MSG_CON_REPLY, 
										      88)
		self.write_buf_addr  =  w_buf_addr
		self.write_buf_size  =  w_buf_size
		self.read_buf_addr   =  r_buf_addr  
		self.read_buf_size	 =  r_buf_size
		self.write_buf_name  =  w_buf_name
		self.read_buf_name   =  r_buf_name


	def encode_msg(self):
		"""
			Encode the content of the message into binary form.
		"""
		msg_beg, vals = super(ConReplyMessage, self).encode_msg()
		code_val   = "".join([msg_beg, "QIQI", HASH_CODE, HASH_CODE])
		
		
		# set the required values
		vals.append(self.write_buf_addr)
		vals.append(self.write_buf_size)
		vals.append(self.read_buf_addr)
		vals.append(self.read_buf_size)
		

		# encode chars
		for char_name in self.write_buf_name:
			vals.append(ord(char_name))
		
		for char_pass in self.read_buf_name:
			vals.append(ord(char_pass))

		# done
		return struct.pack(code_val, *vals)



	
	def decode_msg(self, message):
		"""
			Decode the passsed binary message into the fields
			of the message.
		"""
		self.write_buf_addr, self.write_buf_size,\
		self.read_buf_addr, self.read_buf_size =\
				struct.unpack('=QIQI', message[0:24:1])

		# now unpack the rest of the chars
		mem_names = struct.unpack('='+HASH_CODE+HASH_CODE, message[24::1])
		
		# decode binary strings to Python strings
		self.write_buf_name =\
		"".join([str(chr(ch_item)) for ch_item\
								   in mem_names[0:HASH_LENGTH:1]])
		self.read_buf_name  =\
		"".join([str(chr(ch_item)) for ch_item\
								   in mem_names[HASH_LENGTH::1]])
		
	

@InterMessage.register_subclass(MSG_CLOSE)
class CloseMessage(InterMessage):
	"""
		Session close message.
	"""
	def __init__(self):
		super(CloseMessage, self).__init__(MSG_CLOSE,0)
		    

	def encode_msg(self):
		"""
			Encode the content of the message into binary form.
		"""
		msg_beg, vals = super(CloseMessage, self).encode_msg()

		# done
		return struct.pack(msg_beg, *vals)



	
	def decode_msg(self, message):
		"""
			Decode the passsed binary message into the fields
			of the message.
		"""
		pass


@InterMessage.register_subclass(MSG_DELETE)
class DeleteMessage(InterMessage):
	"""
		Delete object request message.
	"""
	def __init__(self, data_path=""):
		super(DeleteMessage, self).__init__(MSG_DELETE, 
				      						len(data_path) + 2)
		self.data_path = data_path


	def encode_msg(self):
		"""
			Encode the content of the message into binary form.
		"""
		msg_beg, vals = super(DeleteMessage, self).encode_msg()
		string_val = "B"*len(self.data_path)
		code_val = "".join([msg_beg, "H", string_val])
		vals.append(len(self.data_path)) # append length value
		
		# encode chars
		for char_name in self.data_path:
			vals.append(ord(char_name))
		
		# append property bitmap
		vals.append(self.prop_bitmap)

		# done
		return struct.pack(code_val, *vals)



	
	def decode_msg(self, message):
		"""
			Decode the passsed binary message into the fields
			of the message.
		"""
		path_length = struct.unpack("=H", message[0:2:1])
		path_length = path_length[0] # tuple to int
		chars = "B"*path_length
		# now unpack the rest of the chars
		vals  = struct.unpack("="+chars, message[2:2+path_length:1])
		
		# decode a binary string to a Python string
		self.data_path =\
		"".join([str(chr(ch_item)) for ch_item in vals[0::1]])



@InterMessage.register_subclass(MSG_RELEASE)
class ReleaseMessage(InterMessage):
	"""
		Shared memory release notification message.
	"""
	def __init__(self, data_path="", merge_id=0):
		super(ReadMessage, self).__init__(MSG_RELEASE, 
										  len(data_path) + 6)
		self.data_path = data_path
		self.merge_id  = merge_id


	def encode_msg(self):
		"""
			Encode the content of the message into binary form.
		"""
		msg_beg, vals = super(ReleaseMessage, self).encode_msg()
		string_val    = "B"*len(self.data_path)
		code_val      = "".join([msg_beg, "H", string_val, "I"])
		vals.append(len(self.data_path)) # append length value
		
		# encode chars
		for char_name in self.data_path:
			vals.append(ord(char_name))
		
		# append merge ID
		vals.append(self.merge_id)

		# done
		return struct.pack(code_val, *vals)



	
	def decode_msg(self, message):
		"""
			Decode the passsed binary message into the fields
			of the message.
		"""
		path_length = struct.unpack("=H", message[0:2:1])
		path_length = path_length[0] # tuple to int
		chars = "B"*path_length
		# now unpack the rest of the chars
		vals  = struct.unpack("="+chars, message[2:2+path_length:1])
		
		# decode a binary string to a Python string
		self.data_path =\
		"".join([str(chr(ch_item)) for ch_item in vals[0::1]])

		# decode the merge ID
		tmp_bit  = struct.unpack("=I", 
						message[2+path_length:6+path_length:1])
		self.merge_id = tmp_bit[0] # tuple to int

