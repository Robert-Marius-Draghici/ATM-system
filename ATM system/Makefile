PORT = 8000
IP_SERVER = 127.0.0.1
USERS=users_data_file

build: server client

server: 
	gcc -g -Wall server.c -o server

client: 
	gcc -g -Wall client.c -o client

run_server:
	./server ${PORT} $(USERS)

run_client:
	./client ${IP_SERVER} ${PORT}

clean: 
	rm -rf server client
