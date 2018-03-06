# File contains inter-process communication messages described
# in the 'Inter-Processing Message Structrute' document on github.com


# Dependency packages

# Python std
import struct


# singstorage packages 
from . utils import encoding


# MESSAGE TYPE VALUES
MSG_STATUS     =  0
MSG_AUTH       =  1
MSG_READ       =  2
MSG_WRITE      =  3
MSG_CON_REPLY  =  4



HASH_LENGTH = 64
HASH_CODE = "c"*HASH_LENGTH # hash string value
class InterMessage(object):
	"""
		Inter-process communication message base.
	"""
	def __init__(self, msg_type, msg_length):
		self.msg_type   = msg_type
		self.msg_length = msg_length + 5


	def encode_msg(self):
		"""
			Write the style, the type, and length.
		"""
		return ("=BI", [self.msg_type, self.msg_length])


	def decode_msg(self, message):
		self.msg_type, self.msg_length =\
						struct.unpack("=BI", message[0:5:1])


	def get_msg_length(self):
		return self.msg_length




class AuthMessage(InterMessage):
	"""
		Authentication message.
	"""
	def __init__(self, username="", passwd=""):
		super(AuthMessage, self).__init__(MSG_AUTH, 
										  len(username)+HASH_LENGHT + 2)
		self.user_name    = username
		self.passwd_hash  = passwd      


	def encode_msg(self):
		"""
			Encode the content of the message into binary form.
		"""
		msg_beg, vals = super.encode_msg()
		string_val = "c"*len(self.user_name)
		code_val = msg_beg.join(["H", string_val, HASH_CODE])
		vals.append(len(self.user_name)) # append length value
		
		# encode chars
		for char_name in self.user_name:
			vals.append(char_name)
		
		for char_pass in self.passwd_hash:
			vals.append(char_pass)

		# done
		return struct.pack(code_val, *vals)



	
	def decode_msg(self, message):
		"""
			Decode the passsed binary message into the fields
			of the message.
		"""
		user_lenght = struct.unapck("=H", message[0:2:1])
		chars = "c"*(user_lenght+HASH_LENGTH)
		# now unpack the rest of the chars
		name_pass = struct.unpack("="+chars, message[2::1])
		tmp_name = b"".join(list(name_pass[0:user_lenght:1]))
		tmp_pass = b"".join(list(name_pass[user_length::1]))
	
	
		# decode binary strings to strings
		self.user_name   = encoding.decode_to_string(tmp_name)
		self.passwd_hash = encoding.decode_to_string(tmp_pass)
		
		
		

class ReadMessage(InterMessage):
	"""
		Read request message.
	"""
	def __init__(self, data_path="", properties=0):
		super(ReadMessage, self).__init__(MSG_READ, 
										  len(data_path) + 4)
		self.data_path    = data_path
		self.prop_bitmap  = 0


	def encode_msg(self):
		"""
			Encode the content of the message into binary form.
		"""
		msg_beg, vals = super.encode_msg()
		string_val = "c"*len(self.data_path)
		code_val = msg_beg.join(["H", string_val, "I"])
		vals.append(len(self.data_path)) # append length value
		
		# encode chars
		for char_name in self.data_path:
			vals.append(char_name)
		
		# append property bitmap
		vals.append(self.prop_bitmap)

		# done
		return struct.pack(code_val, *vals)



	
	def decode_msg(self, message):
		"""
			Decode the passsed binary message into the fields
			of the message.
		"""
		path_length = struct.unapck("=H", message[0:2:1])
		chars = "c"*path_length
		# now unpack the rest of the chars
		vals  = struct.unpack("="+chars, message[2:2+path_length:1])
		tmp_path = b"".join(list(vals[0:user_lenght:1]))
		
		# decode a binary string to a Python string
		self.data_path = encoding.decode_to_string(tmp_path)

		# decode the property bitmap
		self.prop_bitmap = struct.unpack("=I", 
								  message[2+path_length:6+path_lenght:1])
		
		
	
class WriteMessage(InterMessage):
	"""
		Write request message.
	"""
	def __init__(self,  data_path="", properties=0, 
				 start_addr=0, data_lenght=0):
		super(WriteMessage, self).__init__(MSG_WRITE, 
										  len(data_path) + 22)
		self.data_path    =  data_path
		self.prop_bitmap  =  0
		self.mem_addr     =  start_addr
		self.data_length  =  data_length    


	def encode_msg(self):
		"""
			Encode the content of the message into binary form.
		"""
		msg_beg, vals = super.encode_msg()
		string_val = "c"*len(self.data_path)
		code_val = msg_beg.join(["H", string_val, "IQQ"])
		vals.append(len(self.data_path)) # append lenght value
		
		# encode chars
		for char_name in self.user_name:
			vals.append(char_name)
	
		# append property bitmap, memory address and data lenght	
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
		path_length = struct.unapck("=H", message[0:2:1])
		chars = "c"*path_length
		# now unpack the rest of the chars
		path_data = struct.unpack("="+chars, message[2:path_length+2:1])
		tmp_path = b"".join(list(path_data))
	
		# decode a binary string to a Python string
		self.data_path   = encoding.decode_to_string(tmp_path)
		
		# decode the rest of the message
		self.prop_bitmap, self.mem_addr, self.data_length =\
			struct.unpack("=IQQ", message[2+path_length:24+path_length:1])
		
		
	

class StatusMessage(InterMessage):
	"""
		Operation status message.
	"""
	def __init__(self, status_type=-1):
		super(AuthMessage, self).__init__(MSG_STATUS, 
										  2)
		self.op_status    = status_type
		    

	def encode_msg(self):
		"""
			Encode the content of the message into binary form.
		"""
		msg_beg, vals = super.encode_msg()
		code_val = msg_beg.join(["H"])
		vals.append(self.op_status)

		# done
		return struct.pack(code_val, *vals)



	
	def decode_msg(self, message):
		"""
			Decode the passsed binary message into the fields
			of the message.
		"""
		self.op_status = struct.unpack("=H", message[0:2:1])
		
		
	
class ConReplyMessage(InterMessage):
	"""
		Connection request  message.
	"""
	def __init__(self, w_buf_addr=0,  w_buf_size=0,
				       r_buf_addr=0,  r_buf_size=0,
                       w_buf_name="", r_buf_name=""):
		super(ConReplyMessage, self).__init__(MSG_CON_REPLY, 
										      152)
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
		msg_beg, vals = super.encode_msg()
		string_val = 'c'*len(self.user_name)
		code_val   = msg_beg.join(["QIQI", HASH_CODE, HASH_CODE])
		
		
		# set the required values
		vals.append(self.write_buf_addr)
		vals.append(self.write_buf_size)
		vals.append(self.read_buf_addr)
		vals.append(self.read_buf_size)
		

		# encode chars
		for char_name in self.write_buf_name:
			vals.append(char_name)
		
		for char_pass in self.read_buf_name:
			vals.append(char_pass)

		# done
		return struct.pack(code_val, *vals)



	
	def decode_msg(self, message):
		"""
			Decode the passsed binary message into the fields
			of the message.
		"""
		self.write_buf_addr, self.write_buf_size,\
		self.read_buf_addr, self.read_buf_size =\
				struct.unapck('=QIQI', message[0:24:1])

		# now unpack the rest of the chars
		mem_names = struct.unpack('='+HASH_CODE+HAHS_CODE, message[24::1])
		tmp_write = b"".join(list(mem_names[0:HASH_LENGHT:1]))
		tmp_read  = b"".join(list(name_pass[HASH_LENGHT:(HASH_LENGHT < 1):1]))
		self.write_buf_name = encoding.decode_to_string(tmp_write)
		self.read_buf_name  = encoding.decode_to_string(tmp_read)
		
		
	
