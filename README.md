# multithreaded-network-server
A multithreaded network server in C, using standard TCP connections on top of IPv4. 

The server supports connecting with multiple clients simultaneously as well as multiple simultaneous connections from the same client for increased transfer speeds.

## How To Run the Program 

First, to compile the application, please enter **make** in the original file directory. 

Then, after compiling the files, to run the program, enter:

**./server &lt;configuration file&gt;**
  
The argument &lt;configuration file&gt; will be a path to a binary configuration file.
  
For the program to run correctly, the data of this file need to be arranged in the following layout from the start:
 
- 4 bytes - the IPv4 address the server will listen on, in network byte order,
- 2 bytes - representing the TCP port number the server will listen on, in network byte order,
- All remaining bytes until the end of the file will be ASCII representing the relative or absolute path to the directory (“the target directory”), from which the server will offer files to clients. Not NULL terminated.

After a connection has been established, the slient can send multiple different requests to the server, for which the server will respond accordingly. 

## Compression Option 
The server supports lossless compression and decompression of responses and requests. The compression and decompression algorithms work according to a given compression dictionary file. This file "compression.dict" needs to exist in the same directory as the program executable.
 
### Compression Dictionary Structure 
  
The compression dictionary defines the mapping of bytes to bit codes and consists of 256 segments of variable length. Each segment corresponds in order to input byte values from 0x00 to 0xFF. Each segment is not necessarily aligned to a byte boundary. However, at the end of the 256 segments, there is padding with 0 bits to the next byte boundary.
