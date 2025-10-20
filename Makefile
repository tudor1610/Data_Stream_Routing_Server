CFLAGS = -Wall -g -Werror -Wno-error=unused-variable

all: server subscriber

topic_match.o: topic_match.cpp topic_match.h
	$(CXX) $(CFLAGS) -c topic_match.cpp -o topic_match.o

# Compiling server.cpp
server: server.cpp topic_match.o
	$(CXX) $(CFLAGS) server.cpp topic_match.o -o server

# Compiling subscriber.cpp
subscriber: subscriber.cpp
	$(CXX) $(CFLAGS) subscriber.cpp -o subscriber

.PHONY: clean common run_server run_subscriber

clean:
	rm -f server subscriber *.o