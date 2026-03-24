all:
	g++ -lpthread -Iinclude src/main.cpp src/connect_context.cpp src/http_request.cpp  -o server
build:
	mkdir -p build && cd build && cmake .. && make
clear:
	rm -rf build

# src/connect_context.cpp