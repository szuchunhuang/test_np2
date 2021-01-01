all:server1 server2

server1:np_simple.cpp
	g++ --std=c++11 np_simple.cpp -o np_simple

server2:np_single_proc.cpp
	g++ --std=c++11 np_single_proc.cpp -o np_single_proc

clean:
	rm np_single_proc np_simple