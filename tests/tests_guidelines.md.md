# Testing Guidelines

## Overview

This document describes how to run and write tests for the LeftHookRoll project. All tests are automated through a shell script that handles compilation, execution, and result logging.

## Running Tests

### Prerequisites

- `c++` compiler (must support C++98 standard)
- `bash` shell
- For memory checking: `valgrind` (optional, used with the `valgrind` option)

### Quick Start

Navigate to the tests directory and run the test suite:

```bash
cd tests
./tester.sh
```

### Test Modes

The test runner supports two execution modes:

#### Standard Mode (Default)

Runs all tests with normal execution:

```bash
./tester.sh standard
# or simply
./tester.sh
```

This mode:
- Compiles all test files
- Executes each test normally
- Displays real-time output
- Logs results to `test_results.log`

#### Valgrind Mode

Runs all tests under valgrind memory checker:

```bash
./tester.sh valgrind
```

This mode:
- Compiles all test files
- Runs each test with valgrind to check for memory leaks
- Provides detailed memory analysis
- Logs all results to `test_results.log`

**Note:** Valgrind must be installed on your system. Install it with:
- Ubuntu/Debian: `sudo apt-get install valgrind`
- macOS: `brew install valgrind`

### Help and Usage

View the help message:

```bash
./tester.sh help
# or
./tester.sh -h
./tester.sh --help
```

## Output

### Console Output

The test runner provides colored output for easy reading:
- 🟢 **Green**: Successful operations and passed tests
- 🔴 **Red**: Failures
- 🟡 **Yellow**: Information and headers

Example output:
```
========================================
Running Tests in standard Mode
========================================

[INFO] Compiling test_config...
[PASS] Compiled test_config
[INFO] Running test_config (standard)...
  [PASS] default bitmap is 0
  [PASS] GET not allowed by default
  ...
[PASS] Test passed: test_config

========================================
Test Summary
========================================
Total:  1
Passed: 1
Failed: 0
```

### Log File

All output is logged to `test_results.log` with timestamps:

```bash
cat test_results.log
```

Example log entry:
```
[2026-03-01 10:15:23] Starting test suite in standard mode
[2026-03-01 10:15:23] [INFO] Compiling test_config...
[2026-03-01 10:15:24] [PASS] Compiled test_config
```

## Writing Tests

### Test File Structure

Create test files in the `tests/` directory with the naming convention `test_*.cpp`.

#### Example Structure

```cpp
#include <iostream>
#include <cassert>
#include "../includes/YourHeader.hpp"

static int g_total = 0;
static int g_passed = 0;

static void check(const char* label, bool condition)
{
    g_total++;
    if (condition)
    {
        g_passed++;
        std::cout << "  [PASS] " << label << "\n";
    }
    else
    {
        std::cout << "  [FAIL] " << label << "\n";
    }
}

static void testFeature()
{
    std::cout << "\n-- Feature Name --\n";
    check("test description", condition);
    check("another test", another_condition);
}

int main()
{
    testFeature();

    std::cout << "\n===========================\n";
    std::cout << g_passed << " / " << g_total << " tests passed\n";
    std::cout << "===========================\n";

    return (g_passed == g_total) ? 0 : 1;
}
```

### Best Practices

1. **Naming**: Use `test_*.cpp` for test file names and `test*()` for test functions
2. **Assertions**: Use the `check()` function to verify conditions
3. **Console Output**: Print descriptive test names and results
4. **Exit Code**: Return 0 on all tests passed, 1 on any failure
5. **Group Tests**: Organize related tests into functions (e.g., `testConfigParser()`)
6. **Error Testing**: Test both success and failure paths

### Example Test File

See [test_config.cpp](test_config.cpp) for a complete example with multiple test groups including:
- AllowedMethods tests
- LocationConf tests
- ServerConf tests
- ConfigParser tests
- Error handling tests

## How the Test Runner Works

### Test Discovery

The script automatically finds all `test_*.cpp` files in the tests directory using:
```bash
find tests/ -maxdepth 1 -name "test_*.cpp" -type f
```

### Compilation

Each test file is compiled with:
1. All compiler flags from the project (Wall, Wextra, Werror, C++98, Wshadow)
2. Include path pointing to `includes/` directory
3. All source files from `src/` except `main.cpp`

### Execution

Tests are executed sequentially, and results are captured and logged.

### Results Collection

The script tracks:
- Total number of tests compiled and run
- Number of passed tests
- Number of failed tests
- Detailed output for each test

## Directory Structure

```
tests/
├── tester.sh              # Test runner script
├── test_config.cpp        # Configuration parser tests
├── webserv.conf          # Test configuration file
├── test_results.log      # Generated log file
└── tests_guidelines.md   # This file
```

## Troubleshooting

### Tests Won't Run

1. Check executable permissions:
   ```bash
   chmod +x tester.sh
   ```

2. Ensure you're in the tests directory:
   ```bash
   cd tests
   ```

### Compilation Errors

- Verify all header files exist in `includes/`
- Check that source files are not corrupted
- Ensure C++98 compatibility (no C++11+ features)

### Valgrind Issues

1. If valgrind is not installed, install it first
2. Valgrind may report false positives on some systems
3. Review the full output in `test_results.log`

## Examples

### Running all tests normally
```bash
./tester.sh
```

### Running tests with memory checking
```bash
./tester.sh valgrind
```

### Viewing test results
```bash
tail -f test_results.log        # Watch live updates
cat test_results.log             # View all logged tests
grep FAIL test_results.log      # Find failed tests
```

### Running from project root
```bash
cd /path/to/LeftHookRoll
tests/tester.sh standard
```

## Integration with CI/CD

To use in continuous integration:

```bash
#!/bin/bash
cd tests
if ./tester.sh standard; then
    echo "All tests passed"
    exit 0
else
    echo "Tests failed"
    exit 1
fi
```

## Questions?

For issues with specific tests, examine:
1. The test's source code in the `.cpp` file
2. The full output in `test_results.log`
3. Any compiler error messages
