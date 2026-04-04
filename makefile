# Makefile for Polygon Simplification Project

CXX          = g++
CXXFLAGS     = -std=c++17 -O2 -Wall -Wextra
TARGET       = simplify
VALIDATE     = validate
SRC          = src/SimplifyPolygon.cpp
VALIDATE_SRC = src/validate.cpp
TC           = test_cases

all: $(TARGET) $(VALIDATE)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

$(VALIDATE): $(VALIDATE_SRC)
	$(CXX) $(CXXFLAGS) -o $(VALIDATE) $(VALIDATE_SRC)

clean:
	rm -f $(TARGET) $(TARGET).exe $(VALIDATE) $(VALIDATE).exe _out.txt _val.txt

# ─── Tests ────────────────────────────────────────────────────────────────────

define test_case
./$(TARGET) $(TC)/input_$(1).csv $(2) > _out.txt 2>&1; \
diff --strip-trailing-cr -q $(TC)/output_$(1).txt _out.txt > /dev/null 2>&1 \
&& echo "PASS: $(1)" || echo "FAIL: $(1)"
endef

define check_case
if ./$(TARGET) $(TC)/input_$(1).csv $(2) | ./$(VALIDATE) $(TC)/input_$(1).csv $(2) > _val.txt 2>&1; then \
    echo "PASS: $(1)"; \
else \
    echo "FAIL: $(1)"; cat _val.txt; \
fi
endef

test: $(TARGET)
	@echo "Running all test cases..."
	@$(call test_case,rectangle_with_two_holes,7)
	@$(call test_case,cushion_with_hexagonal_hole,13)
	@$(call test_case,blob_with_two_holes,17)
	@$(call test_case,wavy_with_three_holes,21)
	@$(call test_case,lake_with_two_islands,17)
	@$(call test_case,original_01,99)
	@$(call test_case,original_02,99)
	@$(call test_case,original_03,99)
	@$(call test_case,original_04,99)
	@$(call test_case,original_05,99)
	@$(call test_case,original_06,99)
	@$(call test_case,original_07,99)
	@$(call test_case,original_08,99)
	@$(call test_case,original_09,99)
	@$(call test_case,original_10,99)
	@rm -f _out.txt

check: $(TARGET) $(VALIDATE)
	@echo "Validating all test cases..."
	@$(call check_case,rectangle_with_two_holes,7)
	@$(call check_case,cushion_with_hexagonal_hole,13)
	@$(call check_case,blob_with_two_holes,17)
	@$(call check_case,wavy_with_three_holes,21)
	@$(call check_case,lake_with_two_islands,17)
	@$(call check_case,original_01,99)
	@$(call check_case,original_02,99)
	@$(call check_case,original_03,99)
	@$(call check_case,original_04,99)
	@$(call check_case,original_05,99)
	@$(call check_case,original_06,99)
	@$(call check_case,original_07,99)
	@$(call check_case,original_08,99)
	@$(call check_case,original_09,99)
	@$(call check_case,original_10,99)
	@rm -f _val.txt

.PHONY: all clean test check
