CXX      = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra
TARGET   = simulator
SRC      = main.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	./$(TARGET) input_task2__1_.txt

clean:
	rm -f $(TARGET)
