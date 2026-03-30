# Makefile for Polygon Simplification Project

CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra

TARGET = simplify
SRC = SimplifyPolygon.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

.PHONY: all clean