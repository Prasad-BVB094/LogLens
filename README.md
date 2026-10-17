# LogLens

A C++17 command-line application that analyzes application logs, detects recurring failures, suggests probable root causes, evaluates SLA status, and stores results in SQLite.

## Features

- Log parsing and filtering by severity, service, and date.
- Error and service statistics.
- Rule-based incident detection.
- Rule-based probable root-cause analysis.
- SLA evaluation.
- SQLite persistence.
- Automated unit and integration tests.

## Requirements

- A C++17-compatible compiler, such as GCC 9+, Clang 10+, or MSVC 2019+.
- CMake 3.16 or later.
- SQLite development libraries and headers:
  - Debian/Ubuntu: `libsqlite3-dev`
  - macOS: `sqlite` via Homebrew
  - Windows: SQLite development libraries configured for your compiler.

GoogleTest is required to build and run the automated test suite. CMake may use an installed GoogleTest package or fetch the configured version when network access is available.

To build only the application without tests, configure with `-DBUILD_TESTING=OFF`.

## Build

### Build the application and test suite

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

### Build only the application

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build --parallel
```

## Usage

The executable is generated in the `build/` directory. Multi-configuration generators, such as Visual Studio, may place it in a configuration-specific subdirectory.

### Display help

```bash
./build/loglens --help
```

### Analyze a log file and save results to SQLite

```bash
./build/loglens analyze data/sample.log --db loglens.db
```

### View failure statistics

```bash
./build/loglens stats --db loglens.db
```

### Filter log records

```bash
./build/loglens filter --level ERROR --db loglens.db
./build/loglens filter --service DB --db loglens.db
./build/loglens filter --date 2026-08-25 --db loglens.db
```

### List detected incidents

```bash
./build/loglens incidents --db loglens.db
./build/loglens incidents --status RESOLVED --db loglens.db
```

### Inspect a specific incident

```bash
./build/loglens incident INC-001 --db loglens.db
```

### Generate a report

```bash
./build/loglens report --db loglens.db
```

### View database metadata

```bash
./build/loglens db-info --db loglens.db
```

The commands above assume a Unix-like shell and an executable at `build/loglens`. Adjust the executable path for your platform or build configuration.

## Tests

Tests are enabled by default when supported by the project configuration. After building with tests enabled, run:

```bash
ctest --test-dir build --output-on-failure
```

Individual test executables can also be run directly from `build/tests/` when they have been built:

```bash
./build/tests/test_parser
./build/tests/test_statistics
./build/tests/test_incident_detector
./build/tests/test_root_cause
./build/tests/test_sla
./build/tests/test_database
./build/tests/test_cli
./build/tests/test_integration
```

## Project Structure

```text
LogLens/
├── CMakeLists.txt
├── CMakePresets.json
├── config/
│   ├── detection.conf
│   └── root_cause.conf
├── data/
│   └── sample.log
├── include/
│   └── loglens/
│       ├── Application.h
│       ├── ArgumentParser.h
│       ├── DatabaseManager.h
│       ├── Incident.h
│       ├── IncidentDetector.h
│       ├── LogEntry.h
│       ├── LogParser.h
│       ├── ReportGenerator.h
│       ├── RootCauseEngine.h
│       ├── Severity.h
│       ├── SLAAnalyzer.h
│       ├── StatisticsAnalyzer.h
│       └── Timestamp.h
├── src/
│   ├── Application.cpp
│   ├── ArgumentParser.cpp
│   ├── DatabaseManager.cpp
│   ├── IncidentDetector.cpp
│   ├── LogParser.cpp
│   ├── ReportGenerator.cpp
│   ├── RootCauseEngine.cpp
│   ├── SLAAnalyzer.cpp
│   ├── StatisticsAnalyzer.cpp
│   ├── Timestamp.cpp
│   └── main.cpp
├── tests/
│   ├── CMakeLists.txt
│   ├── test_cli.cpp
│   ├── test_database.cpp
│   ├── test_incident_detector.cpp
│   ├── test_integration.cpp
│   ├── test_parser.cpp
│   ├── test_root_cause.cpp
│   ├── test_sla.cpp
│   └── test_statistics.cpp
└── .github/
    └── workflows/
        └── ci.yml
```

## Limitations

- LogLens performs offline, batch analysis of existing log files. It does not monitor running operating system processes, consume live log streams in real time, or inspect network traffic.
- Root-cause analysis uses deterministic pattern matching against error messages. Its results are heuristic suggestions, not definitive proof of causality.
- The parser expects log records in the format `YYYY-MM-DD HH:MM:SS LEVEL SERVICE MESSAGE`.
