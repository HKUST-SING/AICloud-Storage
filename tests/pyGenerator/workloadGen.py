# This file is used to generate workload.
# Workload will output to a file with following
# format.
# Above format entry, there are description about
# the whole workload, including total request,
# write ratio, the distribution of read.
# Description begin with '#' to seperate workload
# entries.

# Format
# Read:
# 0,object name
# Write:
# 1,object name,object size

# Dependency
import numpy as np

OBJPREFIX = "/obj"

def getReadObj(totalObj,readObjDistribution,a):
	if readObjDistribution == "zipf":
		return np.random.zipf(a) % totalObj
	else:
		return np.around(np.random.uniform()*1000).astype('int') % totalObj

# IN:
# `readObjDistribution` only support `uniform` and `zipf`.
# Default value is `uniform`.
# If `readObjDistribution` is `zipf`, `a` should be provided
# and its value is larger than 1.
def genWorkload(outputFile,totalRequests,writeRatio,\
				minSize,maxSize,readObjDistribution="uniform",a=1.5):
	
	# Number of write requests and read requests
	writeRequests,readRequests = \
		np.around([totalRequests * writeRatio,totalRequests * (1-writeRatio)]).astype('int')
	
	# Initialize request
	requests = np.append(np.ones(writeRequests-1),np.zeros(readRequests)).astype('int')

	# Shuffle request
	np.random.shuffle(requests)
	requests = np.append(np.ones(1),requests).astype('int')
	
	# Create write object size, distribution is `uniform`
	writeSizes = np.random.uniform(minSize,maxSize,writeRequests).astype('int')

	# Create workload and output to the file
	with open(outputFile,'w') as output:
		description = "# Total Requests: {0}\tWrite Ratio: {1}\n".format(\
															totalRequests,writeRatio) +\
					  "# Read Requests Distribution: {0}\n".format(readObjDistribution)
		output.writelines(description)
		description = "# Format\n# Read:\n# 0,object name\n" +\
					  "# Write:\n# 1,object name,object size\n\n"
		output.writelines(description)
		# Print each request
		totalObj = 1
		for req in requests:
			objname = OBJPREFIX
			objsize = ","
			if req == 0:
				objname = "{0}{1}".format(\
								objname,getReadObj(totalObj,readObjDistribution,a))
				objsize = ""
			else:
				objname = "{0}{1}".format(objname,totalObj-1)
				objsize = "{0}{1}".format(objsize,writeSizes[totalObj-1])
				totalObj = totalObj + 1
			entry = "{0},{1}{2}\n".format(req,objname,objsize)
			output.writelines(entry)

if __name__ == '__main__':
	genWorkload("file.txt",101,0.4,4,8)
