# Contributing to LogLens

Contributions to LogLens are welcome. Please follow these guidelines to keep the code consistent and maintainable.

## Code Standards

- **Language:** Use C++17.
- **Compiler warnings:** Avoid compiler warnings. Use `-Wall -Wextra -Wpedantic` when building with GCC or Clang.
- **Formatting:** Use 4 spaces for C++ indentation and 2 spaces for CMake, YAML, and JSON.
- **Resource management:** Prefer RAII and standard library types. Avoid raw owning pointers and unnecessary manual memory management.
- **Testing:** Add GoogleTest unit or integration tests for new features, bug fixes, and relevant edge cases.
- **Architecture:** Keep SQL operations within the database layer. Analytical modules should not execute SQL statements directly.

## Pull Request Checklist

Before submitting a pull request:

1. Build the project successfully.
2. Run the existing test suite and verify that the tests pass.
3. Add or update unit and integration tests where appropriate.
4. Summarize user-visible changes and notable fixes in `CHANGELOG.md`, when applicable.
5. Describe relevant architectural changes and the reasoning behind them in the pull request description.
