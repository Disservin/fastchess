CXX = g++

all:
	$(CXX) -O3 -march=native *.cpp -o fast-chess 