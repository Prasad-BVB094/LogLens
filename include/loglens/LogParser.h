#pragma once

#include "loglens/LogEntry.h"

#include <string>
#include <vector>
#include <string_view>
#include <optional>
#include <istream>

namespace loglens {

enum class ParserMode {
    Tolerant,
    Strict
};

struct ParseResult {
    std::vector<LogEntry> entries{};
    std::size_t total_lines{0};
    std::size_t valid_records{0};
    std::size_t malformed_records{0};
    std::vector<std::string> diagnostics{};
};

class LogParser {
public:
    explicit LogParser(ParserMode mode = ParserMode::Tolerant) noexcept : mode_(mode) {}

    [[nodiscard]] ParserMode mode() const noexcept { return mode_; }
    void set_mode(ParserMode mode) noexcept { mode_ = mode; }

    // Parses a single line. Returns std::nullopt if the line is blank or malformed.
    // If line is blank, is_blank output flag is set to true.
    std::optional<LogEntry> parse_line(
        std::string_view line,
        std::size_t line_number,
        std::string* error_detail = nullptr,
        bool* is_blank = nullptr) const;

    // Parses an entire stream line by line
    ParseResult parse_stream(std::istream& is) const;

    // Parses a file from disk
    ParseResult parse_file(const std::string& filepath) const;

private:
    ParserMode mode_{ParserMode::Tolerant};
};

} // namespace loglens
