#pragma once

#include "loglens/ArgumentParser.h"
#include <iostream>

namespace loglens {

class Application {
public:
    Application(std::ostream& out = std::cout, std::ostream& err = std::cerr) noexcept
        : out_(out), err_(err) {}

    int run(int argc, const char* const argv[]);

private:
    std::ostream& out_;
    std::ostream& err_;

    int handle_analyze(const ParsedOptions& opts);
    int handle_stats(const ParsedOptions& opts);
    int handle_filter(const ParsedOptions& opts);
    int handle_incidents(const ParsedOptions& opts);
    int handle_incident_detail(const ParsedOptions& opts);
    int handle_report(const ParsedOptions& opts);
    int handle_db_info(const ParsedOptions& opts);
};

} // namespace loglens
