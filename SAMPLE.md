Client
------
````
$ ./kftclient 127.0.0.1 4001 sample.txt output.txt 100 0
Starting transfer...
Transfer complete.
Transfer took 0 second(s) and 7 millisecond(s)
````

Server
------
````
$ ./kftserver 4001 0   
Server started...
127.0.0.1 - Received request for sample.txt
127.0.0.1 - Starting transfer
127.0.0.1 - Transfer complete
127.0.0.1 - Terminating connection
````
