.PHONY: test clean

BIN = a.out

CXX = g++
INC_DIR = ../../firmware/arduino/shapodice

CPP_FILES = $(wildcard ./*.cpp)
C_FILES = $(wildcard ./*.c)

EXTRA_DEPENDENCIES = \
	Makefile \
	$(wildcard $(INC_DIR)/*.*)

test: $(BIN)
	./$(BIN)

$(BIN): $(C_FILES) $(CPP_FILES) $(EXTRA_DEPENDENCIES)
	$(CXX) -o $@ $(C_FILES) $(CPP_FILES) -I$(INC_DIR)

clean:
	rm -f $(BIN)
