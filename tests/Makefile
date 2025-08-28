# Makefile pour les tests AI-OS

# Configuration de base
CC = gcc
CFLAGS_BASE = -Wall -Wextra -std=c99 -I. -I.. -I../kernel -I../include
CFLAGS_KERNEL = $(CFLAGS_BASE) -DUSE_INLINE_ASM -g
CFLAGS_USER = $(CFLAGS_BASE) -g

# Répertoires
TEST_DIR = .
FRAMEWORK_DIR = $(TEST_DIR)/framework
UNIT_DIR = $(TEST_DIR)/unit
INTEGRATION_DIR = $(TEST_DIR)/integration
SYSTEM_DIR = $(TEST_DIR)/system
PERFORMANCE_DIR = $(TEST_DIR)/performance
ROBUSTNESS_DIR = $(TEST_DIR)/robustness
BUILD_DIR = ../build
LOG_DIR = ../test_logs

# Fichiers du framework
FRAMEWORK_SOURCES = $(FRAMEWORK_DIR)/unity.c $(FRAMEWORK_DIR)/test_kernel.c $(FRAMEWORK_DIR)/kernel_mocks.c
FRAMEWORK_HEADERS = $(FRAMEWORK_DIR)/unity.h $(FRAMEWORK_DIR)/test_kernel.h

# Listes des tests
KERNEL_TESTS = $(wildcard $(UNIT_DIR)/kernel/test_*.c)
USERSPACE_TESTS = $(wildcard $(UNIT_DIR)/userspace/test_*.c)
FS_TESTS = $(wildcard $(UNIT_DIR)/fs/test_*.c)
INTEGRATION_TESTS = $(wildcard $(INTEGRATION_DIR)/test_*.c)
SYSTEM_TESTS = $(wildcard $(SYSTEM_DIR)/test_*.c)
PERFORMANCE_TESTS = $(wildcard $(PERFORMANCE_DIR)/test_*.c)
ROBUSTNESS_TESTS = $(wildcard $(ROBUSTNESS_DIR)/test_*.c)

# Binaires de test
KERNEL_TEST_BINS = $(KERNEL_TESTS:%.c=$(BUILD_DIR)/%)
USERSPACE_TEST_BINS = $(USERSPACE_TESTS:%.c=$(BUILD_DIR)/%)
FS_TEST_BINS = $(FS_TESTS:%.c=$(BUILD_DIR)/%)
INTEGRATION_TEST_BINS = $(INTEGRATION_TESTS:%.c=$(BUILD_DIR)/%)
SYSTEM_TEST_BINS = $(SYSTEM_TESTS:%.c=$(BUILD_DIR)/%)
PERFORMANCE_TEST_BINS = $(PERFORMANCE_TESTS:%.c=$(BUILD_DIR)/%)
ROBUSTNESS_TEST_BINS = $(ROBUSTNESS_TESTS:%.c=$(BUILD_DIR)/%)

ALL_TEST_BINS = $(KERNEL_TEST_BINS) $(USERSPACE_TEST_BINS) $(FS_TEST_BINS) \
                $(INTEGRATION_TEST_BINS) $(SYSTEM_TEST_BINS) \
                $(PERFORMANCE_TEST_BINS) $(ROBUSTNESS_TEST_BINS)

# Cibles principales
.PHONY: all clean test test-quick test-kernel test-userspace test-integration \
        test-performance test-robustness help setup

all: $(ALL_TEST_BINS)
	@echo "All test binaries compiled successfully"

# Cible par défaut
test: all
	@echo "Running complete test suite..."
	@bash ./scripts/run_all_tests.sh

# Tests rapides (critiques seulement)
test-quick: $(KERNEL_TEST_BINS)
	@echo "Running quick test suite..."
	@bash ./scripts/run_all_tests.sh --quick

# Tests par catégorie
test-kernel: $(KERNEL_TEST_BINS)
	@echo "Running kernel tests..."
	@bash ./scripts/run_all_tests.sh --kernel-only

test-userspace: $(USERSPACE_TEST_BINS)
	@echo "Running userspace tests..."
	@bash ./scripts/run_all_tests.sh --userspace-only

test-integration: $(INTEGRATION_TEST_BINS)
	@echo "Running integration tests..."
	@bash ./scripts/run_all_tests.sh --integration

test-performance: $(PERFORMANCE_TEST_BINS)
	@echo "Running performance tests..."
	@bash ./scripts/run_all_tests.sh --performance

test-robustness: $(ROBUSTNESS_TEST_BINS)
	@echo "Running robustness tests..."
	@bash ./scripts/run_all_tests.sh --robustness

# Compilation des tests kernel
$(BUILD_DIR)/$(UNIT_DIR)/kernel/test_%: $(UNIT_DIR)/kernel/test_%.c $(FRAMEWORK_SOURCES) $(FRAMEWORK_HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS_KERNEL) -o $@ $< $(FRAMEWORK_SOURCES)
	@echo "Compiled kernel test: $(notdir $@)"

# Compilation des tests userspace
$(BUILD_DIR)/$(UNIT_DIR)/userspace/test_%: $(UNIT_DIR)/userspace/test_%.c $(FRAMEWORK_SOURCES) $(FRAMEWORK_HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS_USER) -o $@ $< $(FRAMEWORK_SOURCES)
	@echo "Compiled userspace test: $(notdir $@)"

# Compilation des tests filesystem
$(BUILD_DIR)/$(UNIT_DIR)/fs/test_%: $(UNIT_DIR)/fs/test_%.c $(FRAMEWORK_SOURCES) $(FRAMEWORK_HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS_KERNEL) -o $@ $< $(FRAMEWORK_SOURCES)
	@echo "Compiled filesystem test: $(notdir $@)"

# Compilation des tests d'intégration
$(BUILD_DIR)/$(INTEGRATION_DIR)/test_%: $(INTEGRATION_DIR)/test_%.c $(FRAMEWORK_SOURCES) $(FRAMEWORK_HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS_KERNEL) -o $@ $< $(FRAMEWORK_SOURCES)
	@echo "Compiled integration test: $(notdir $@)"

# Compilation des tests système
$(BUILD_DIR)/$(SYSTEM_DIR)/test_%: $(SYSTEM_DIR)/test_%.c $(FRAMEWORK_SOURCES) $(FRAMEWORK_HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS_KERNEL) -o $@ $< $(FRAMEWORK_SOURCES)
	@echo "Compiled system test: $(notdir $@)"

# Compilation des tests de performance
$(BUILD_DIR)/$(PERFORMANCE_DIR)/test_%: $(PERFORMANCE_DIR)/test_%.c $(FRAMEWORK_SOURCES) $(FRAMEWORK_HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS_KERNEL) -o $@ $< $(FRAMEWORK_SOURCES)
	@echo "Compiled performance test: $(notdir $@)"

# Compilation des tests de robustesse
$(BUILD_DIR)/$(ROBUSTNESS_DIR)/test_%: $(ROBUSTNESS_DIR)/test_%.c $(FRAMEWORK_SOURCES) $(FRAMEWORK_HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS_KERNEL) -o $@ $< $(FRAMEWORK_SOURCES)
	@echo "Compiled robustness test: $(notdir $@)"

# Tests individuels
run-test-%: $(BUILD_DIR)/*/test_%
	@echo "Running individual test: $*"
	@$<

# Test avec Valgrind (si disponible)
test-valgrind: $(USERSPACE_TEST_BINS)
	@echo "Running tests with Valgrind..."
	@for test in $(USERSPACE_TEST_BINS); do \
		echo "Valgrind: $$test"; \
		valgrind --leak-check=full --show-leak-kinds=all $$test || true; \
	done

# Génération de rapports
coverage-report:
	@echo "Generating coverage report..."
	@mkdir -p $(LOG_DIR)
	@echo "Coverage analysis not yet implemented" > $(LOG_DIR)/coverage_report.txt

test-report:
	@echo "Generating test report..."
	@mkdir -p $(LOG_DIR)
	@./scripts/generate_test_report.sh > $(LOG_DIR)/test_summary.html

# Benchmarks
benchmark: $(PERFORMANCE_TEST_BINS)
	@echo "Running performance benchmarks..."
	@mkdir -p $(LOG_DIR)
	@echo "Benchmark Results - $(shell date)" > $(LOG_DIR)/benchmark_results.txt
	@for test in $(PERFORMANCE_TEST_BINS); do \
		echo "Running benchmark: $$test"; \
		$$test >> $(LOG_DIR)/benchmark_results.txt 2>&1 || true; \
	done
	@echo "Benchmark results saved to $(LOG_DIR)/benchmark_results.txt"

# Installation et setup
setup:
	@echo "Setting up test environment..."
	@mkdir -p $(BUILD_DIR) $(LOG_DIR)
	@chmod +x scripts/*.sh
	@echo "Test environment ready"

install-deps:
	@echo "Installing test dependencies..."
	@sudo apt-get update
	@sudo apt-get install -y build-essential gcc-multilib valgrind
	@echo "Dependencies installed"

# Nettoyage
clean:
	@echo "Cleaning test binaries and logs..."
	@rm -rf $(BUILD_DIR)/$(TEST_DIR)
	@rm -f $(LOG_DIR)/test_results_*.log
	@echo "Clean completed"

clean-all: clean
	@echo "Cleaning all generated files..."
	@rm -rf $(BUILD_DIR) $(LOG_DIR)
	@echo "Complete clean finished"

# Debug et développement
debug-test-%: $(BUILD_DIR)/*/test_%
	@echo "Running test $* with debug output..."
	@gdb -batch -ex run -ex bt --args $<

list-tests:
	@echo "Available tests:"
	@echo "Kernel Tests:"
	@for test in $(KERNEL_TESTS); do echo "  - $$(basename $$test .c)"; done
	@echo "Userspace Tests:"
	@for test in $(USERSPACE_TESTS); do echo "  - $$(basename $$test .c)"; done
	@echo "Integration Tests:"
	@for test in $(INTEGRATION_TESTS); do echo "  - $$(basename $$test .c)"; done
	@echo "Performance Tests:"
	@for test in $(PERFORMANCE_TESTS); do echo "  - $$(basename $$test .c)"; done

check-deps:
	@echo "Checking dependencies..."
	@which gcc >/dev/null || (echo "ERROR: gcc not found" && exit 1)
	@which gcc-multilib >/dev/null 2>&1 || echo "WARNING: gcc-multilib not found (needed for 32-bit compilation)"
	@which valgrind >/dev/null 2>&1 || echo "INFO: valgrind not found (optional for memory testing)"
	@echo "Dependencies check completed"

# Intégration continue
ci-test: setup clean all test
	@echo "Continuous Integration test completed"

# Aide
help:
	@echo "AI-OS Test Framework - Available targets:"
	@echo ""
	@echo "Building:"
	@echo "  all              - Compile all test binaries"
	@echo "  setup            - Setup test environment"
	@echo "  install-deps     - Install required dependencies"
	@echo ""
	@echo "Testing:"
	@echo "  test             - Run complete test suite"
	@echo "  test-quick       - Run critical tests only"
	@echo "  test-kernel      - Run kernel tests only"
	@echo "  test-userspace   - Run userspace tests only"
	@echo "  test-integration - Run integration tests only"
	@echo "  test-performance - Run performance tests only"
	@echo "  test-robustness  - Run robustness tests only"
	@echo "  test-valgrind    - Run tests with Valgrind"
	@echo ""
	@echo "Individual tests:"
	@echo "  run-test-<name>  - Run specific test"
	@echo "  debug-test-<name>- Debug specific test with GDB"
	@echo ""
	@echo "Reports:"
	@echo "  coverage-report  - Generate coverage report"
	@echo "  test-report      - Generate HTML test report"
	@echo "  benchmark        - Run performance benchmarks"
	@echo ""
	@echo "Utilities:"
	@echo "  list-tests       - List all available tests"
	@echo "  check-deps       - Check dependencies"
	@echo "  clean            - Clean test binaries"
	@echo "  clean-all        - Clean all generated files"
	@echo "  ci-test          - Continuous integration test"
	@echo "  help             - Show this help"
	@echo ""
	@echo "Examples:"
	@echo "  make test                    # Run all tests"
	@echo "  make test-kernel             # Run only kernel tests"
	@echo "  make run-test-pmm           # Run PMM test specifically"
	@echo "  make debug-test-task        # Debug task test with GDB"

# Validation des fichiers
validate:
	@echo "Validating test files..."
	@for test_file in $(KERNEL_TESTS) $(USERSPACE_TESTS) $(FS_TESTS); do \
		echo "Checking syntax: $$test_file"; \
		$(CC) $(CFLAGS_BASE) -fsyntax-only $$test_file 2>/dev/null || \
		echo "WARNING: Syntax issues in $$test_file"; \
	done
	@echo "Validation completed"

# Statistiques
stats:
	@echo "Test Suite Statistics:"
	@echo "  Kernel tests: $(words $(KERNEL_TESTS))"
	@echo "  Userspace tests: $(words $(USERSPACE_TESTS))"
	@echo "  Filesystem tests: $(words $(FS_TESTS))"
	@echo "  Integration tests: $(words $(INTEGRATION_TESTS))"
	@echo "  System tests: $(words $(SYSTEM_TESTS))"
	@echo "  Performance tests: $(words $(PERFORMANCE_TESTS))"
	@echo "  Robustness tests: $(words $(ROBUSTNESS_TESTS))"
	@echo "  Total tests: $(words $(ALL_TEST_BINS))"
	@echo ""
	@echo "Framework files:"
	@echo "  Sources: $(words $(FRAMEWORK_SOURCES))"
	@echo "  Headers: $(words $(FRAMEWORK_HEADERS))"

# Règles de dépendance
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(LOG_DIR):
	@mkdir -p $(LOG_DIR)

# S'assurer que les répertoires existent
$(ALL_TEST_BINS): | $(BUILD_DIR) $(LOG_DIR)

# Empêcher la suppression des fichiers intermédiaires
.PRECIOUS: $(ALL_TEST_BINS)

# Marquer les cibles qui ne créent pas de fichiers
.PHONY: $(filter-out $(ALL_TEST_BINS), $(MAKECMDGOALS))
