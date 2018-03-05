import singstorage.singexcept   as errors
import singstorage.io_ops       as ops
import singstorage.usercontext  as context



# per user session context
cloud_user = None


def connect(username, password):

    global cloud_user
    if cloud_user: # already connected 
        raise errors.AuthError("User connected.")

    # initialize a user and try to connect to the storage service
    cloud_user = context.UserCtx(username, password)
    ops.connect_to_cluster(cloud_user)


def write_data(dpath, data):
    ops.write_data_sync(cloud_user, dpath, data)
      

def read_data(dpath):
    return ops.read_data_sync(cloud_user, dpath)


def close():
    ops.close_conn(cloud_user)
