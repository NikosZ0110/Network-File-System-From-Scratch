# Network-File-System-From-Scratch
A low-level network file system implemented in C/Java using UDP, semaphores, and concurrent client-server communication.

Client runs on JVM, server requires compilation using GNU GCC.

Run client: ```java App.java <server_IP_address> <port> <num_of_cache_blocks> <refresh_time>```
Port is set to 8080 by default in server

Run server: ```gcc server.c server_lib.c -o server
               ./server <path_to_root_directory_to_store_data>```

Client instructions for input:
* `o <filename>` - open a file
* `r <fd> <String> <int>` - read <int> bytes from <filename>
* `w <fd>` - write <String> string in <filename> in position where cursor is located
* `l <fd> <long> <int>` - move cursor of <filename> <long> bytes at <String> (String -> "start", "end", "here")
* `c <fd>` - close <filename>

NFS uses caching in client size for optimization and incarnation numbers and UDP for data packet sending and validating
