#include "loglens/LogParser.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <cctype>

namespace loglens {

namespace {

inline std::string_view trim_right(std::string_view s) noexcept {
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ' || s.back() == '\t')) {
        s.remove_suffix(1);
    }
    return s;
}

inline bool is_all_whitespace(std::string_view s) noexcept {
    for (char c : s) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    return true;
}

} // namespace

std::optional<LogEntry> LogParser::parse_line(
    std::string_view raw_line,
    std::size_t line_number,
    std::string* error_detail,
    bool* is_blank) const {

    if (is_blank) *is_blank = false;

    std::string_view line = trim_right(raw_line);

    if (line.empty() || is_all_whitespace(line)) {
        if (is_blank) *is_blank = true;
        return std::nullopt;
    }

    // Must be at least 19 chars for "YYYY-MM-DD HH:MM:SS"
    if (line.size() < 20 || line[19] != ' ') {
        if (error_detail) {
            *error_detail = "Line " + std::to_string(line_number) + ": Malformed timestamp prefix or missing delimiter";
        }
        return std::nullopt;
    }

    std::string_view ts_str = line.substr(0, 19);
    auto ts_opt = Timestamp::parse(ts_str);
    if (!ts_opt) {
        if (error_detail) {
            *error_detail = "Line " + std::to_string(line_number) + ": Invalid calendar timestamp '" + std::string(ts_str) + "'";
        }
        return std::nullopt;
    }

    // Position after timestamp space
    std::size_t pos = 20;
    while (pos < line.size() && line[pos] == ' ') {
        ++pos;
    }

    if (pos >= line.size()) {
        if (error_detail) {
            *error_detail = "Line " + std::to_string(line_number) + ": Missing severity level and service fields";
        }
        return std::nullopt;
    }

    // Extract LEVEL
    std::size_t level_start = pos;
    while (pos < line.size() && line[pos] != ' ') {
        ++pos;
    }
    std::string_view level_str = line.substr(level_start, pos - level_start);
    auto sev_opt = parse_severity(level_str);
    if (!sev_opt) {
        if (error_detail) {
            *error_detail = "Line " + std::to_string(line_number) + ": Unknown severity level '" + std::string(level_str) + "'";
        }
        return std::nullopt;
    }

    // Skip whitespace to SERVICE
    while (pos < line.size() && line[pos] == ' ') {
        ++pos;
    }

    if (pos >= line.size()) {
        if (error_detail) {
            *error_detail = "Line " + std::to_string(line_number) + ": Missing service identifier";
        }
        return std::nullopt;
    }

    // Extract SERVICE
    std::size_t service_start = pos;
    while (pos < line.size() && line[pos] != ' ') {
        ++pos;
    }
    std::string_view service_str = line.substr(service_start, pos - service_start);

    // Skip whitespace to MESSAGE
    while (pos < line.size() && line[pos] == ' ') {
        ++pos;
    }

    if (pos >= line.size()) {
        if (error_detail) {
            *error_detail = "Line " + std::to_string(line_number) + ": Missing log message";
        }
        return std::nullopt;
    }

    // Rest of the line is MESSAGE (preserving inner and trailing text)
    std::string_view message_str = line.substr(pos);

    return LogEntry{
        line_number,
        *ts_opt,
        *sev_opt,
        std::string(service_str),
        std::string(message_str)
    };
}

ParseResult LogParser::parse_stream(std::istream& is) const {
    ParseResult result;
    std::string line;
    std::size_t line_no = 0;

    while (std::getline(is, line)) {
        ++line_no;
        ++result.total_lines;

        std::string error;
        bool blank = false;
        auto entry = parse_line(line, line_no, &error, &blank);

        if (blank) {
            continue;
        }

        if (entry) {
            result.entries.push_back(std::move(*entry));
            ++result.valid_records;
        } else {
            ++result.malformed_records;
            result.diagnostics.push_back(error);
            if (mode_ == ParserMode::Strict) {
                throw std::runtime_error("Strict parse error at " + error);
            }
        }
    }

    return result;
}

ParseResult LogParser::parse_file(const std::string& filepath) const {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        if (mode_ == ParserMode::Strict) {
            throw std::runtime_error("Cannot open log file: " + filepath);
        }
        ParseResult res;
        res.diagnostics.push_back("Error: Cannot open log file '" + filepath + "'");
        return res;
    }

    return parse_stream(file);
}

} // namespace loglens
