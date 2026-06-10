CC = g++
CFLAGS = -std=c++20 -Wall -pthread

SRCS = MapReduceJob.cpp MapContext.cpp ReduceContext.cpp
OBJS = $(SRCS:.cpp=.o)
LIB = MapReduceJob.a

.PHONY: all clean

all: $(LIB) tests/test01_word_count tests/test02_is_done_after_wait SampleClient

$(LIB): $(OBJS)
	ar rcs $@ $^

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

tests/test01_word_count: tests/test01_word_count.cpp $(LIB)
	$(CC) $(CFLAGS) -I. $< $(LIB) -o $@

tests/test02_is_done_after_wait: tests/test02_is_done_after_wait.cpp $(LIB)
	$(CC) $(CFLAGS) -I. $< $(LIB) -o $@

SampleClient: sample_client/SampleClient.cpp $(LIB)
	$(CC) $(CFLAGS) -I. $< $(LIB) -o $@

clean:
	rm -f *.o $(LIB) tests/test01_word_count tests/test02_is_done_after_wait SampleClient
