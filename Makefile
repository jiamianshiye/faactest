INCLUDE=
INCLUDE+=./

CC=g++

#FILES = $(subst ./, , $(foreach dir,.,$(wildcard $(dir)/*.c)) )
#OBJS = $(subst .c,.o, $(FILES))

FILES_C=$(wildcard *.c)
#FILES_C=$()
FILES_CPP=$(wildcard *.cpp)
FILES=$(FILES_C)
FILES+=$(FILES_CPP)
OBJS=
OBJS+=$(subst .c,.o, $(FILES_C))
OBJS+=$(subst .cpp,.o, $(FILES_CPP))


CC_OPTS=-g -c -Wall
LD_FLAGS= -lpthread -L"/usr/local/faac/lib" -lfaac -lpulse

EXE=test

all: $(EXE)

$(EXE):$(OBJS)
	$(CC)  -o $@ $(OBJS) $(LD_FLAGS) 

%.o:%.cpp
	@echo \# $(FILES) : files list....
	@echo \# $(OBJS) : files list....
	$(CC) 	$(CC_OPTS) $(INCLUDE) $(LD_FLAGS) $< -o $@	



.PHONY: clean

clean:
	-rm *.o
	-rm $(EXE)