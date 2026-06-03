# ==========================================
# COMPILER & FLAGS
# ==========================================
CXX = clang++

# -O3: Maximum execution speed optimization
# -march=native: Optimizes for the exact CPU architecture building the code (crucial for L1 cache/instruction alignment)
# -pthread: Required for std::thread and pthread_setaffinity_np
# -Iinclude: Tells the compiler to look in the /include directory for headers
# -Wall -Wextra: Turns on all standard warnings to catch bugs early
CXXFLAGS = -std=c++17 -O3 -march=native -pthread -Wall -Wextra -Iinclude

# ==========================================
# DIRECTORIES
# ==========================================
SRC_DIR = src
INC_DIR = include
BUILD_DIR = build

# ==========================================
# FILES & TARGETS
# ==========================================
# Find all .cpp files in the src directory
SRCS = $(wildcard $(SRC_DIR)/*.cpp)

# Map every .cpp file to a .o object file in the build directory
OBJS = $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRCS))

# The final executable name
TARGET = $(BUILD_DIR)/engine

# ==========================================
# BUILD RULES
# ==========================================
# Default rule: typing 'make' runs this
all: directories $(TARGET)

# Create the build directory if it doesn't exist
directories:
	@mkdir -p $(BUILD_DIR)

# Link all object files to create the final executable
$(TARGET): $(OBJS)
	@echo "[LINKING] Creating executable $@"
	$(CXX) $(CXXFLAGS) -o $@ $^

# Compile each .cpp file into a .o file
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@echo "[COMPILING] $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ==========================================
# UTILITY RULES
# ==========================================
# Clean up compiled files
clean:
	@echo "[CLEANING] Removing build files"
	rm -rf $(BUILD_DIR)/*

# Compile and run immediately
run: all
	@echo "[RUNNING] Executing $(TARGET)..."
	@echo "----------------------------------------"
	@./$(TARGET)

# Phony targets to prevent conflicts with files named "clean" or "run"
.PHONY: all directories clean run