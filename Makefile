CFLAGS = -Wall -g -Werror -Wno-error=unused-variable g++

COMMON_FILES = common.cpp

all: server subscriber
.PHONY: clean

%.o: %.cpp
	$(CFLAGS) -c $<

server: server.cpp common.cpp
	g++ -c common.cpp
	g++ -o server server.cpp common.cpp -Wall

subscriber: subscriber.cpp $(COMMON_FILES:.cpp=.o)
	g++ -o subscriber subscriber.cpp $(COMMON_FILES:.cpp=.o)

clean:
	rm -rf server subscriber common *.o *.dSYM