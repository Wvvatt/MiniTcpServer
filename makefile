CXX:=g++

TARGET=./mini
SRC=$(wildcard *.cpp)
OBJ:=$(SRC:.cpp=.o)
INCLUDE=
DEP_LIB_PATH=
DEP_LIB=

CXXFALG=-std=c++11 -g

all:$(TARGET)

$(TARGET):$(OBJ)
	$(CXX) $(CXXFALG) -o $(TARGET) $(OBJ)$(DEP_LIB_PATH) $(DEP_LIB)
	
%.o:%.cpp
	$(CXX) $(CXXFALG) -o $@ -c $< $(INCLUDE)

.PHONY: clean
clean:
	rm -f ${TARGET} 