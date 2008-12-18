This directory includes binaries and embeddable source code from Google Protocol Buffers
http://code.google.com/p/protobuf/ 
http://code.google.com/p/protobuf-c/ 

The binaries were build from unmodified protobuf 2.0.3 and protobuf-c 0.6 sources, targeted at:
    - Mac OS X 10.5

The source (see src/) directory, is an extraction of the protobuf-c runtime library. It
has been modified as follows:
    - Unintialized value compiler warnings were fixed, and marked
      with "landonf - 12/17/2008 (uninitialized compiler warning))"
    - Use __LITTLE_ENDIAN__ to determine host endian-ness.
