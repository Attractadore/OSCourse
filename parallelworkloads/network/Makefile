CFLAGS = -std=c99 -DNETDEBUG -O3
CXXFLAGS = -std=c++11 -DNETDEBUG -O3
LIBS = -lpthread
CC = gcc
CXX = g++

all: TrapezoidClient TrapezoidServer

clean:
	-rm -f *.o TrapezoidClient TrapezoidServer

NetworkCommon.o: NetworkCommon.c
	$(CC) $(CFLAGS) -c NetworkCommon.c $(LIBS)

NetworkClient.o: NetworkClient.c
	$(CC) $(CFLAGS) -c NetworkClient.c $(LIBS)

NetworkServer.o: NetworkServer.c
	$(CC) $(CFLAGS) -c NetworkServer.c $(LIBS)

TrapezoidServer: NetworkServer.o NetworkCommon.o
	$(CXX) $(CPPFLAGS) NetworkServer.o NetworkCommon.o CPUTopology.cpp ScheduleTrapezoid.cpp TrapezoidServer.cpp -o TrapezoidServer $(LIBS)

TrapezoidClient: NetworkClient.o NetworkCommon.o
	$(CXX) $(CPPFLAGS) NetworkClient.o NetworkCommon.o CPUTopology.cpp ScheduleTrapezoid.cpp TrapezoidClient.cpp -o TrapezoidClient $(LIBS)

