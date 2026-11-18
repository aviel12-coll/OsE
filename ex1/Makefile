# Makefile for uthreads library and tests

CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++11 -g
LDFLAGS = -lm

# Directories
SRC_DIR = .
TEST_DIR = test
BUILD_DIR = build

# Source files
UTHREADS_SRC = $(SRC_DIR)/uthreads.cpp
UTHREADS_OBJ = $(BUILD_DIR)/uthreads.o

# Test files
TEST_BASIC = test_basic_scheduler
TEST_BASIC_SRC = $(TEST_DIR)/test_basic_scheduler.cpp
TEST_BASIC_OBJ = $(BUILD_DIR)/test_basic_scheduler.o
TEST_BASIC_BIN = $(BUILD_DIR)/test_basic_scheduler

# Default target
all: $(BUILD_DIR) $(TEST_BASIC_BIN)

# Create build directory
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# Compile uthreads.cpp
$(UTHREADS_OBJ): $(UTHREADS_SRC)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile test_basic_scheduler.cpp
$(TEST_BASIC_OBJ): $(TEST_BASIC_SRC)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Link test_basic_scheduler
$(TEST_BASIC_BIN): $(UTHREADS_OBJ) $(TEST_BASIC_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

# Run test
run-basic: $(TEST_BASIC_BIN)
	@echo "Running basic scheduler test..."
	$(TEST_BASIC_BIN)

# Clean build artifacts
clean:
	@rm -rf $(BUILD_DIR)
	@echo "Cleaned build directory"

# Help
help:
	@echo "Available targets:"
	@echo "  make all         - Build all tests"
	@echo "  make run-basic   - Run basic scheduler test"
	@echo "  make clean       - Clean build artifacts"
	@echo "  make help        - Show this help message"

.PHONY: all run-basic clean help
