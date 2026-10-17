# Changelog

Notable changes to LogLens are documented in this file.

This changelog follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/) and uses [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2026-10-16

### Added

- C++17 command-line application for log parsing, failure statistics, incident detection, and probable root-cause analysis.
- Log parser with calendar validation and strict/tolerant parsing modes.
- Rolling time-window incident detection with configurable thresholds and severity handling.
- Service recovery matching and SLA evaluation based on severity-specific time limits.
- Rule-based root-cause analysis with priority scoring and fallback classification.
- SQLite persistence for storing analysis results.
- GoogleTest unit and integration test suites for parsing, statistics, incident detection, root-cause analysis, SLA evaluation, database operations, CLI behavior, and integration workflows.
- CMake-based build configuration.
- GitHub Actions workflow for automated builds and testing across supported platforms.

