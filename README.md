bloom
=====

A simple efficient program that uses libcalg to find duplicates.

You can either use CMake or make; this project supports both.

In the future, I will probably remove support for the later.

Working on a monitoring deamon that uses libnotify.

libraries
=========

bloom plans to leverage:

* tclap (for argument parsing)
* c-algorithms (for data structures)
* libcrypt (for computing file hashes)
* syslog (for reporting and monitoring)
* gdbm (for persistence of file information)
* zlib (for compression of various data)
