#include "loglens/Timestamp.h"

#include <cstdio>
#include <array>

namespace loglens {

bool Timestamp::is_leap_year(int year) noexcept {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int Timestamp::days_in_month(int year, int month) noexcept {
    if (month < 1 || month > 12) return 0;
    static constexpr std::array<int, 13> kDays = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && is_leap_year(year)) {
        return 29;
    }
    return kDays[static_cast<std::size_t>(month)];
}

// Days from civil year/month/day since Unix epoch (1970-01-01)
// Uses Howard Hinnant's algorithm for fast civil-from-days / days-from-civil
static std::int64_t days_from_civil(int y, unsigned m, unsigned d) noexcept {
    y -= m <= 2;
    const std::int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);                  // [0, 399]
    const unsigned doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;       // [0, 365]
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;                 // [0, 146096]
    return era * 146097 + static_cast<std::int64_t>(doe) - 719468;
}

static std::tuple<int, unsigned, unsigned> civil_from_days(std::int64_t z) noexcept {
    z += 719468;
    const std::int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(z - era * 146097);               // [0, 146096]
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365; // [0, 399]
    const std::int64_t y = static_cast<std::int64_t>(yoe) + era * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);               // [0, 365]
    const unsigned mp = (5 * doy + 2) / 153;                                    // [0, 11]
    const unsigned d = doy - (153 * mp + 2) / 5 + 1;                            // [1, 31]
    const unsigned m = mp < 10 ? mp + 3 : mp - 9;                               // [1, 12]
    return {static_cast<int>(y + (m <= 2)), m, d};
}

std::optional<Timestamp> Timestamp::parse(std::string_view sv) noexcept {
    // Expected format: YYYY-MM-DD HH:MM:SS (length exactly 19)
    if (sv.size() != 19) {
        return std::nullopt;
    }

    if (sv[4] != '-' || sv[7] != '-' || sv[10] != ' ' || sv[13] != ':' || sv[16] != ':') {
        return std::nullopt;
    }

    auto parse2 = [](char a, char b) -> int {
        if (a < '0' || a > '9' || b < '0' || b > '9') return -1;
        return (a - '0') * 10 + (b - '0');
    };

    auto parse4 = [](char a, char b, char c, char d) -> int {
        if (a < '0' || a > '9' || b < '0' || b > '9' ||
            c < '0' || c > '9' || d < '0' || d > '9') return -1;
        return (a - '0') * 1000 + (b - '0') * 100 + (c - '0') * 10 + (d - '0');
    };

    const int year = parse4(sv[0], sv[1], sv[2], sv[3]);
    const int month = parse2(sv[5], sv[6]);
    const int day = parse2(sv[8], sv[9]);
    const int hour = parse2(sv[11], sv[12]);
    const int minute = parse2(sv[14], sv[15]);
    const int second = parse2(sv[17], sv[18]);

    if (year < 1970 || year > 2200) return std::nullopt;
    if (month < 1 || month > 12) return std::nullopt;
    if (day < 1 || day > days_in_month(year, month)) return std::nullopt;
    if (hour < 0 || hour > 23) return std::nullopt;
    if (minute < 0 || minute > 59) return std::nullopt;
    if (second < 0 || second > 59) return std::nullopt;

    const std::int64_t days = days_from_civil(year, static_cast<unsigned>(month), static_cast<unsigned>(day));
    const std::int64_t total_seconds = days * 86400 + static_cast<std::int64_t>(hour * 3600 + minute * 60 + second);

    return Timestamp(total_seconds);
}

std::string Timestamp::to_string() const {
    const std::int64_t days = epoch_seconds_ >= 0 ? epoch_seconds_ / 86400 : (epoch_seconds_ - 86399) / 86400;
    std::int64_t rem_seconds = epoch_seconds_ - days * 86400;
    if (rem_seconds < 0) rem_seconds += 86400;

    const unsigned hours = static_cast<unsigned>(rem_seconds / 3600);
    const unsigned minutes = static_cast<unsigned>((rem_seconds % 3600) / 60);
    const unsigned seconds = static_cast<unsigned>(rem_seconds % 60);

    const auto [y, m, d] = civil_from_days(days);

    char buf[64];
    std::snprintf(buf, sizeof(buf), "%04d-%02u-%02u %02u:%02u:%02u", y, m, d, hours, minutes, seconds);
    return std::string(buf);
}

std::string Timestamp::date_only() const {
    const std::int64_t days = epoch_seconds_ >= 0 ? epoch_seconds_ / 86400 : (epoch_seconds_ - 86399) / 86400;
    const auto [y, m, d] = civil_from_days(days);

    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d-%02u-%02u", y, m, d);
    return std::string(buf);
}

} // namespace loglens
