# -*- coding: utf-8 -*-

from __future__ import absolute_import
from __future__ import unicode_literals
from __future__ import division
from __future__ import print_function

# Custom logging module. The file is only used for testing the package
# to collect information about the system.


# Python std
import sys
import logging
logging.basicConfig(stream=sys.stderr, level=logging.DEBUG)

PACK_LOG_LEVEL=logging.DEBUG
