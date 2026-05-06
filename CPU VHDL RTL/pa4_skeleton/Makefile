# === Variables ===
GHDL       = ghdl
FLAGS      = --std=08
TOP_ENTITY = testbench
SRC_FILES  = $(wildcard code/src/*.vhd) $(wildcard code/tb/*.vhd)

TEST_DIR   = assembled_tests
LOG_DIR    = Logs
WAVE_DIR   = Waves

# Automatically list all tests in assembled_tests (without extension)
# TESTS = $(basename $(notdir $(wildcard $(TEST_DIR)/*.out)))
TESTS = \
    array_adder      \
    colliding_cache  \
	gemm   		     \
	gemm2  		     \
	gemm3  		     \
	gemm4  		     \
    IType_ADD        \
    IType_LOAD       \
    IType_SLL        \
    IType_SRL        \
    loop             \
    lw_sw_test       \
    matrix_mul_itr   \
    matrix_mul		 \
    no_dep_test      \
    RType_ADD        \
    RType_SLL        \
    RType_SRL        \
    RType_SUB        \
    SType_STORE

# === Default target ===
all: clean analyze
	@echo "Analysis complete. Use 'make dump_all' to run all tests or 'make <test_name>' for individual tests."

# === Analyze all files ===
analyze:
	$(GHDL) -a $(FLAGS) $(SRC_FILES)

# === Pattern rule: individual test names depend on their log files ===
$(TESTS): %: clean $(LOG_DIR)/%.log

# === Run a single test ===
$(LOG_DIR)/%.log: analyze
	@mkdir -p $(LOG_DIR)
	@mkdir -p $(WAVE_DIR)
	@t=$*; \
	echo "------------------------------------------------------------------------------------"; \
	echo "------------------------------- Test: $$t"; \
	echo "------------------------------------------------------------------------------------"; \
	$(GHDL) -r $(FLAGS) $(TOP_ENTITY) \
		-gMEM_INIT_FILE="$(TEST_DIR)/$$t.out" \
		--wave="$(WAVE_DIR)/$$t.ghw" \
		--ieee-asserts=disable; \
	\
	dmem_file="$(LOG_DIR)/$$t"_dmem.log; \
	reg_file="$(LOG_DIR)/$$t"_reg.log; \
	final_file="$(LOG_DIR)/$$t.log"; \
	\
	if [ -f $$dmem_file ] && [ -f $$reg_file ]; then \
		cat $$dmem_file $$reg_file > $$final_file; \
		rm -f $$dmem_file $$reg_file; \
		echo "Generated final log: $$final_file"; \
	else \
		echo "Warning: Missing log files for $$t"; \
	fi

# === Run all tests ===
dump_all: clean analyze $(foreach t,$(TESTS),$(LOG_DIR)/$(t).log)
	@cat $(foreach t,$(TESTS),$(LOG_DIR)/$(t).log) > $(LOG_DIR)/PA4_output.log
# 	@rm -f $(foreach t,$(TESTS),$(LOG_DIR)/$(t).log)
	@echo "All tests concatenated into $(LOG_DIR)/PA4_output.log"

# === Clean ===
clean:
	rm -f *.o *.cf *.ghw $(TOP_ENTITY)
	rm -rf $(LOG_DIR) $(WAVE_DIR)

.PHONY: all analyze dump_all clean $(TESTS)