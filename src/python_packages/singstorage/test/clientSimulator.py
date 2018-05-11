# This is client simulator.
# Simulator will produce requests by calling
# `singstorage` module.
# Requests workload satisfy workload file,
# which is created by `workloadGen`.

import singstorage

READ = 0
WRITE = 1

UNIT = 1 # Byte
LOG_FILE = "log.txt"

# Get the workload list.
# Entry Format:
# [int,string(,int)]
def getWorkload(workload):
	workloadlist = []
	with open(workload,'r') as f:
		entry = f.readline()
		while entry:
			# Decode to string
			#bytes.decode(entry)

			# Entry begins from '#' indicating
			# it is a description
			if entry[0].isdigit():
				# Strip space and '\n'
				entry = entry.lstrip("\n \t")
				entry = entry.rstrip("\n \t")

				# Split entry
				entry = entry.split(',')

				# Translate value from string to int
				entry[0] = int(entry[0])
				# If this write request, resolve the third element
				if entry[0] == WRITE:
					entry[2] = int(entry[2])*UNIT

				# Append to workload list
				workloadlist.append(entry)

			entry = f.readline()

	# Return workload list
	return workloadlist


def process(username,passwd,requestList,logFile=LOG_FILE):
	with open(logFile,'a') as log:
		if singstorage.connect(username,passwd) == singstorage.SUCCESS:
			count = 1
			for entry in requestList:
				try:
					if entry[0] == READ:
						r_handle = singstorage.read_data(entry[1])
						for some_data in r_handle:
							pass
					elif entry[0] == WRITE:
						singstorage.write_data(entry[1],bytearray(b"x"*entry[2]))
				except singstorage.IOException as e:
					log.write("{0}\n".format(e))
				else:
					log.writelines("Finish Request {0}\n".format(count))
					count = count + 1
				finally:
					pass

if __name__ == '__main__':
	wlist = getWorkload("file.txt")

	process(wlist,"/home/czeng/pyGenerator/log")
