#!/bin/bash

# ============================================================================
# Test Runner Script
# Builds and runs all test files in the tests directory
# Supports standard execution and valgrind memory checking
# ============================================================================

set -o pipefail

# Configuration
TESTS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "${TESTS_DIR}")"
INCLUDES_DIR="${PROJECT_ROOT}/includes"
SRC_DIR="${PROJECT_ROOT}/src"
TEST_BUILD_DIR="${TESTS_DIR}/build"
RESULTS_FILE="${TESTS_DIR}/test_results.log"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Compiler settings (matching Makefile)
CXX="c++"
CXXFLAGS="-Wall -Wextra -Werror -std=c++98 -Wshadow -I${INCLUDES_DIR}"

# ============================================================================
# Helper Functions
# ============================================================================

print_usage() {
	cat << EOF
Usage: $0 [OPTIONS]

Options:
  standard                Run tests with standard execution (default)
  valgrind               Run tests with valgrind memory checker
  help                   Display this help message

Examples:
  $0 standard
  $0 valgrind
  $0

Results are logged to: ${RESULTS_FILE}

EOF
}

log_message() {
	local message="$1"
	echo "[$(date '+%Y-%m-%d %H:%M:%S')] $message" | tee -a "${RESULTS_FILE}"
}

print_header() {
	local message="$1"
	echo -e "\n${YELLOW}========================================${NC}"
	echo -e "${YELLOW}$message${NC}"
	echo -e "${YELLOW}========================================${NC}"
	echo "" | tee -a "${RESULTS_FILE}"
	log_message "$message"
}

print_success() {
	echo -e "${GREEN}✓ $1${NC}"
	log_message "[PASS] $1"
}

print_error() {
	echo -e "${RED}✗ $1${NC}"
	log_message "[FAIL] $1"
}

print_info() {
	echo -e "${YELLOW}ℹ $1${NC}"
	log_message "[INFO] $1"
}

# ============================================================================
# Main Functions
# ============================================================================

setup_build_directory() {
	if [ -d "${TEST_BUILD_DIR}" ]; then
		rm -rf "${TEST_BUILD_DIR}"
	fi
	mkdir -p "${TEST_BUILD_DIR}"
}

collect_source_files() {
	# Collect all .cpp files from src directory (excluding main.cpp)
	local src_files=""
	for file in "${SRC_DIR}"/*.cpp; do
		if [ -f "$file" ]; then
			local basename=$(basename "$file")
			if [ "$basename" != "main.cpp" ]; then
				src_files="${src_files} $file"
			fi
		fi
	done
	echo "$src_files"
}

compile_test() {
	local test_file="$1"
	local output_binary="$2"
	local test_name=$(basename "$test_file" .cpp)

	print_info "Compiling $test_name..."

	local src_files=$(collect_source_files)

	if ${CXX} ${CXXFLAGS} "${test_file}" ${src_files} -o "${output_binary}" 2>&1 | tee -a "${RESULTS_FILE}"; then
		print_success "Compiled $test_name"
		return 0
	else
		print_error "Failed to compile $test_name"
		return 1
	fi
}

run_test_standard() {
	local binary="$1"
	local test_name=$(basename "$binary")

	print_info "Running $test_name (standard)..."

	if "${binary}" 2>&1 | tee -a "${RESULTS_FILE}"; then
		print_success "Test passed: $test_name"
		return 0
	else
		print_error "Test failed: $test_name"
		return 1
	fi
}

run_test_valgrind() {
	local binary="$1"
	local test_name=$(basename "$binary")

	print_info "Running $test_name (valgrind)..."

	# Check if valgrind is installed
	if ! command -v valgrind &> /dev/null; then
		print_error "valgrind is not installed"
		return 1
	fi

	if valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
		--verbose "${binary}" 2>&1 | tee -a "${RESULTS_FILE}"; then
		print_success "Test passed: $test_name (valgrind)"
		return 0
	else
		print_error "Test failed: $test_name (valgrind)"
		return 1
	fi
}

run_all_tests() {
	local mode="$1"
	local passed=0
	local failed=0
	local total=0

	# Find all test .cpp files in tests directory
	local test_files=$(find "${TESTS_DIR}" -maxdepth 1 -name "test_*.cpp" -type f | sort)

	if [ -z "$test_files" ]; then
		print_error "No test files found in ${TESTS_DIR}"
		return 1
	fi

	print_header "Running Tests in $mode Mode"

	for test_file in $test_files; do
		local test_name=$(basename "$test_file" .cpp)
		local output_binary="${TEST_BUILD_DIR}/${test_name}"

		((total++))

		# Compile test
		if ! compile_test "$test_file" "$output_binary"; then
			((failed++))
			continue
		fi

		# Run test
		if [ "$mode" = "valgrind" ]; then
			if run_test_valgrind "$output_binary"; then
				((passed++))
			else
				((failed++))
			fi
		else
			if run_test_standard "$output_binary"; then
				((passed++))
			else
				((failed++))
			fi
		fi

		echo ""
	done

	# Print summary
	print_header "Test Summary"
	echo -e "Total:  $total"
	echo -e "${GREEN}Passed: $passed${NC}"
	if [ $failed -gt 0 ]; then
		echo -e "${RED}Failed: $failed${NC}"
	else
		echo -e "${GREEN}Failed: $failed${NC}"
	fi
	echo ""

	log_message "Summary: $passed/$total tests passed"

	return $([ $failed -eq 0 ])
}

cleanup() {
	if [ -d "${TEST_BUILD_DIR}" ]; then
		rm -rf "${TEST_BUILD_DIR}"
	fi
}

# ============================================================================
# Main Entry Point
# ============================================================================

main() {
	local mode="standard"

	# Parse arguments
	if [ $# -gt 0 ]; then
		case "$1" in
			standard)
				mode="standard"
				;;
			valgrind)
				mode="valgrind"
				;;
			help|-h|--help)
				print_usage
				exit 0
				;;
			*)
				echo "Unknown option: $1"
				print_usage
				exit 1
				;;
		esac
	fi

	# Clear previous results
	> "${RESULTS_FILE}"

	print_header "Test Suite Initialization"
	log_message "Starting test suite in $mode mode"
	log_message "Project root: ${PROJECT_ROOT}"
	log_message "Tests directory: ${TESTS_DIR}"
	echo ""

	# Setup
	setup_build_directory

	# Run tests
	if run_all_tests "$mode"; then
		echo -e "${GREEN}All tests passed!${NC}"
		cleanup
		exit 0
	else
		echo -e "${RED}Some tests failed!${NC}"
		cleanup
		exit 1
	fi
}

# Run main function
main "$@"
