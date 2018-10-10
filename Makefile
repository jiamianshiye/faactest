INCLUDE=
INCLUDE+=-I"./"\
		-I"/usr/local/faac/include"

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


CC_OPTS=-g -Wall -c
LD_FLAGS=-std=c++11 -lpthread -lpulse -L"/usr/local/faac/lib" -lfaac 

EXE=test

all:clean $(EXE)

$(EXE):$(OBJS)
	$(CC)  -o $@ $(OBJS)  $(INCLUDE) $(LD_FLAGS) 

.c.o:
	@echo \#files list $(FILES) : ....
	@echo \#files list $(INCLUDE) :....
	@echo \#files list $(OBJS) : ....
	$(CC) 	$(CC_OPTS) $< -o $@	$(LD_FLAGS) 
.cpp.o:
	@echo \#cpp files list $(OBJS) : ....
	$(CC) 	$(CC_OPTS) $< -o $@	$(LD_FLAGS)  $(INCLUDE)


.PHONY: clean

clean:
	-rm *.o
	-rm $(EXE)