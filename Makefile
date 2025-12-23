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

MAIN_SRC = main.cpp
MAIN_OBJ = $(BUILD_DIR)/main.o
TARGET = interpreter

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(MAIN_OBJ) $(RUNTIME_OBJS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(MAIN_OBJ) $(RUNTIME_OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/main.o: $(MAIN_SRC) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(RUNTIME_DIR)/%.c $(RUNTIME_DIR)/%.h $(wildcard $(RUNTIME_DIR)/*.h) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(RUNTIME_DIR) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)