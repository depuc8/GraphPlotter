# Compiler
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -Iinclude $(shell pkg-config --cflags sdl3)
LDLIBS = $(shell pkg-config --libs sdl3)

# Directories
SRC_DIR = src
BUILD_DIR = build

# Files
# Use wildcard to automatically grab ALL .cpp files in the src directory
SRC = $(wildcard $(SRC_DIR)/*.cpp)
OBJ = $(SRC:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
TARGET = plot

# Default target
all: $(TARGET)

# Link executable
$(TARGET): $(OBJ)
	$(CXX) -o $@ $^ $(LDLIBS)

# Compile each source file (with header dependency tracking)
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

-include $(OBJ:.o=.d)

# Remove generated files
clean:
	rm -rf $(BUILD_DIR) $(TARGET)

# Rebuild from scratch
rebuild: clean all

.PHONY: all clean rebuild
