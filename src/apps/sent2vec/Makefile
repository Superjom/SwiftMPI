#CXX=../../../third/local/bin/mpicxx
#CXX=mpicxx
MPICXX=g++ -I/home/chunwei/swiftmpi/third/openmpi-1.8.5/../local/include -pthread -Wl,-rpath -Wl,/home/chunwei/swiftmpi/third/openmpi-1.8.5/../local/lib -Wl,--enable-new-dtags -L/home/chunwei/swiftmpi/third/openmpi-1.8.5/../local/lib -lmpi_cxx -lmpi
CC=gcc
BIN=./bin

CXXFLAGS= -g -O3 -std=c++11 -pthread -lpthread -lgtest -lgtest_main -lzmq -lz -lglog
LOCAL_ROOT=../../../third/local

THIRD_INCPATH=-isystem $(LOCAL_ROOT)/include \
	-I/usr/include/pthread  \
	-I/usr/local/Cellar/google-sparsehash/2.0.2/include 

THIRD_LIB=-L$(LOCAL_ROOT)/lib  

sent2vec : sent2vec.cpp
	mkdir -p $(BIN)
	$(MPICXX) sent2vec.cpp $(THIRD_INCPATH)  -Xlinker $(THIRD_LIB)  $(CXXFLAGS) -o $(BIN)/sent2vec

