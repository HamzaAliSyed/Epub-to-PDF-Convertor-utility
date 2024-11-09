CXX = clang++
CXXFLAGS = -Wall -Wextra -std=c++23 
LDFLAGS = 

BUILD_DIR = build
SRC_DIR = src
TARGET = epubtopdf.exe

all: $(TARGET)

$(TARGET): $(BUILD_DIR)/main.o $(BUILD_DIR)/xmlparser.o
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(TARGET)