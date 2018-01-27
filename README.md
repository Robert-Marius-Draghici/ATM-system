# ATM-system
This is an implementation of a client-server application in the form of an atm system that uses TCP and UDP sockets. The atm is the server and the atm users are the clients. The communication between the server and the clients relies mostly on TCP sockets, the UDP sockets being used just in the case when an user is locked out of his account and needs to unlock it. 

The client can introduce the following commands:
* login <card_number> <pin>
* logout
* listsold (it shows the current amount of money stored)
* getmoney <sum_to_withdraw>
* putmoney <sum_to_deposit>
* unlock
* quit
  
The server can only receive the command quit.

A detailed description of the implementation is offered in the file README in the directory ATM-system.

Usage example:
* compile: make
* run: make run_server and make run_client
