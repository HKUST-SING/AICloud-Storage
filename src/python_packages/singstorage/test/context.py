# This module shall be imported by each of the tests so that the tests
# could have access to the 'singstorage' module.


import os
import sys

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))


# import singstorage package
import singstorage
