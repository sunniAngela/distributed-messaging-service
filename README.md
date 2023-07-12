# distributed-messaging-service
Multi-threaded concurrent client-server messaging service between users using socket programming. This project was developed by a team of three for the  Distributed Systems course during the second semester of our third year at university. Based on a publication/subscription service, both server and client modules were developed.

## Authors
- sunniAngela
- Iarnaiz
- HashHKR

## Server
A multi-threaded concurrent server that provides the communication service between the different clients registered in the system, manages the connections of said clients, and stores the messages sent to a client not directly connected to the system. As this was a simple course project not meant for actual deployment, the selected method of storage is simply a linked list in memory, as implemented through the library `users_llist`.

The basic functionality consists of an infinite loop in which the main thread of the server accepts a connection from a client at every iteration, listens to their request and creates a new thread to process and execute it. The communication is performed through TCP sockets. For each type of request, the server will read from the socket as many times as needed to identify all the components that comprise the request. As the threads will work on a shared resource (lists of users and messages), we need to use mutex to avoid data race conditions. 


## Client
A multi-threaded command-line client that communicates with the server and is able to send and receive messages. One of the threads will be used to send messages to the server and the other to receive them. If a client is disconnected, they cannot send or receive messages. Once they reconnect they will receive all pending menssages that were sent during their absence. Just like Whatsapp!

The commands available to the client are:
- `REGISTER <username>`: to register as a new user
- `UNREGISTER <username>`: to delete a previously registered user
- `CONNECT <username>`: to connect or log in to the system
- `DISCONNECT <username>`: to disconnect or log out of the system
- `SEND <username> <message>`: to send a message to a specific user

After sending each request, the client will get a confirmation message from the server informing whether the operation was successful or not. 

### Spyne server
A Spyne server was used as an introduction in the course to web services. The functionality offered is optional. It returns the message string after removing all extra blank spaces before sending it. To avoid the need of connecting to the Spyne server, it is only needed to comment the invocation of the method `remove_spaces()` in `client.py`. Otherwise, the web service shall be run by executing `server_spyne.py` before running the client's program.

The Spyne server will be listening at port 8080 by default (can be set manually in `client.py`). Dependencies to be installed:

```console
pip install zeep
pip install spyne
```

## Installation
The server side can be compiled using the facilitated *CMakeLists* file in a few simple steps:

```console
cd server
mkdir build
cmake -B build/
cd build/
make
```

## Execution example

Once you've compiled the server program, you can just run it on the desired port. As the Spyne server might be using the 8080 port, we'll run it on the  8081 port:

```console
./server -p 8081
```

If we want to use the Spyne server to remove extra blank spaces in the messages, we must go to the `client/` directory and execute the Python program:

```console
python3 server_spyne.py
```

Then, we can run two clients by opening two terminals and specifying the IP of the server and the port. As we are running it locally, we can just specify *localhost* or 127.0.0.1:

```console
python3 client.py -s localhost -p 8081
```

Client 1:

```console
c> REGISTER boogie33
c> CONNECT boogie33
```

Client 2:

```console
c> REGISTER beastmaster64
c> CONNECT beastmaster64
c> SEND boogie33 hi, wanna hear a joke?
```

Et voilà! User *boogie33* will magically receive the following message from *beastmaster64* on their terminal:

```console
MESSAGE 0 FROM beastmaster64 :
  hi, wanna hear a joke?
END
```

Meanwhile, you can read all the log messages on the server side to track the connections and requests that are happening.

## Files and Directory Structure

The project directory is organized as follows:
- **server/**
    - **server.c**: Source code for the server side
    - **include/**: Contains the custom libraries used in the server
        - **lines.h**: Header for line parsing library
        - **lines.c**: Source code for line parsing library
        - **users_llist.h**: Header for linked list library to store users
        - **users_llist.c**: Source code for linked list library to store users
- **client/**: Contains the program in Python for the client side
    - **client.py**: Source code for the client side
    - **client_spyne.py**: Spyne server for the client to carry out additional blank space removal functionality
