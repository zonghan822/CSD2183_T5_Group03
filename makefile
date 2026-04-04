# Makefile for Polygon Simplification Project

CXX          = g++
CXXFLAGS     = -std=c++17 -O2 -Wall -Wextra
BUILDDIR     = build
OUTDIR       = my_output
TARGET       = $(BUILDDIR)/simplify
VALIDATE     = $(BUILDDIR)/validate
SRC          = src/SimplifyPolygon.cpp
VALIDATE_SRC = src/validate.cpp
TC           = test_cases

all: $(TARGET) $(VALIDATE)

$(TARGET): $(SRC) | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

$(VALIDATE): $(VALIDATE_SRC) | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -o $(VALIDATE) $(VALIDATE_SRC)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(OUTDIR):
	mkdir -p $(OUTDIR)

clean:
	rm -rf $(BUILDDIR) $(OUTDIR)

# ─── Tests ────────────────────────────────────────────────────────────────────

define test_case
$(TARGET) $(TC)/input_$(1).csv $(2) > $(OUTDIR)/output_$(1).txt 2>&1; \
diff --strip-trailing-cr -q $(TC)/output_$(1).txt $(OUTDIR)/output_$(1).txt > /dev/null 2>&1 \
&& echo "PASS: $(1)" || echo "FAIL: $(1)"
endef

define check_case
if $(TARGET) $(TC)/input_$(1).csv $(2) | $(VALIDATE) $(TC)/input_$(1).csv $(2) > $(OUTDIR)/check_$(1).txt 2>&1; then \
    echo "PASS: $(1)"; \
else \
    echo "FAIL: $(1)"; cat $(OUTDIR)/check_$(1).txt; \
fi
endef

test: $(TARGET) | $(OUTDIR)
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

check: $(TARGET) $(VALIDATE) | $(OUTDIR)
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

.PHONY: all clean test check
