FLAGS=-g -pthread 
all: clean server client
	chmod +x server client

client: client.cpp
	g++ ${FLAGS} -std=c++11 -o client client.cpp common.cpp

server: server.cpp
	g++ ${FLAGS} -std=c++11 -o client client.cpp common.cpp
	g++ ${FLAGS} -std=c++11 -o server server.cpp common.cpp

clean:
	rm -f server client
