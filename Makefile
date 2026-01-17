CC = gcc
CXX = g++

CFLAGS = -Wall -Wextra -std=c99 -O2
CXXFLAGS = -Wall -Wextra -std=c++17 -O2
LDFLAGS =

RUNTIME_DIR = runtime
SRC_DIR = .
BUILD_DIR = build

RUNTIME_SRCS = $(wildcard $(RUNTIME_DIR)/*.c)
RUNTIME_OBJS = $(RUNTIME_SRCS:$(RUNTIME_DIR)/%.c=$(BUILD_DIR)/%.o)

COMMON_SRC = commons.cpp
ANALYSER_SRC = analyser.cpp
MAIN_SRC = main.cpp

COMMON_OBJ = $(BUILD_DIR)/commons.o
ANALYSER_OBJ = $(BUILD_DIR)/analyser.o
MAIN_OBJ = $(BUILD_DIR)/main.o

INTERPRETER_TARGET = interpreter
ANALYSER_TARGET = analyser

.PHONY: all clean

all: $(INTERPRETER_TARGET) $(ANALYSER_TARGET)

$(INTERPRETER_TARGET): $(MAIN_OBJ) $(COMMON_OBJ) $(RUNTIME_OBJS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(ANALYSER_TARGET): $(ANALYSER_OBJ) $(COMMON_OBJ) $(RUNTIME_OBJS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(RUNTIME_DIR)/%.c $(RUNTIME_DIR)/%.h $(wildcard $(RUNTIME_DIR)/*.h) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(RUNTIME_DIR) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR) $(INTERPRETER_TARGET) $(ANALYSER_TARGET)