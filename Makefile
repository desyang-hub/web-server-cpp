all:
	g++ -lpthread -Iinclude src/main.cpp src/connect_context.cpp src/http_request.cpp  -o server

# src/connect_context.cpp