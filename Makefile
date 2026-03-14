all:
	g++ -lpthread main.cpp -o server
test:
	g++ -lpthread main.cc -o server