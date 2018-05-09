#!/usr/bin/env python

# -*-coding: utf-8 -*


from distutils.core import setup


setup(name = "singstorage",
	version = "1.0",
	description = "Singstorage Python Package",
	url="https://github.com/HKUST-SING/AICloud-Storage",
    requires = ["future", "six", "posix_ipc"],
	packages = ["singstorage", "singstorage.utils",
				"singstorage.public", "singstorage.internal",
				"singstorage.internal.utils",
				"singstorage.internal.utils.allocators"])
