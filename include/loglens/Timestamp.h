#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <optional>
#include <ostream>
#include <tuple>

namespace loglens {

class Timestamp {
public:
    constexpr Timestamp() noexcept : epoch_seconds_(0) {}
    constexpr explicit Timestamp(std::int64_t seconds) noexcept : epoch_seconds_(seconds) {}

    static std::optional<Timestamp> parse(std::string_view sv) noexcept;
    static Timestamp from_epoch(std::int64_t seconds) noexcept { return Timestamp(seconds); }
    static Timestamp fromEpochSeconds(std::int64_t seconds) noexcept { return from_epoch(seconds); }

    [[nodiscard]] constexpr std::int64_t epoch_seconds() const noexcept { return epoch_seconds_; }

    [[nodiscard]] std::string to_string() const;
    [[nodiscard]] std::string date_only() const;

    [[nodiscard]] constexpr std::int64_t operator-(const Timestamp& other) const noexcept {
        return epoch_seconds_ - other.epoch_seconds_;
    }

    [[nodiscard]] constexpr Timestamp operator+(std::int64_t seconds) const noexcept {
        return Timestamp(epoch_seconds_ + seconds);
    }

    [[nodiscard]] constexpr Timestamp operator-(std::int64_t seconds) const noexcept {
        return Timestamp(epoch_seconds_ - seconds);
    }

    constexpr bool operator<(const Timestamp& other) const noexcept { return epoch_seconds_ < other.epoch_seconds_; }
    constexpr bool operator<=(const Timestamp& other) const noexcept { return epoch_seconds_ <= other.epoch_seconds_; }
    constexpr bool operator>(const Timestamp& other) const noexcept { return epoch_seconds_ > other.epoch_seconds_; }
    constexpr bool operator>=(const Timestamp& other) const noexcept { return epoch_seconds_ >= other.epoch_seconds_; }
    constexpr bool operator==(const Timestamp& other) const noexcept { return epoch_seconds_ == other.epoch_seconds_; }
    constexpr bool operator!=(const Timestamp& other) const noexcept { return epoch_seconds_ != other.epoch_seconds_; }

    static bool is_leap_year(int year) noexcept;
    static int days_in_month(int year, int month) noexcept;

private:
    std::int64_t epoch_seconds_{0};
};

inline std::ostream& operator<<(std::ostream& os, const Timestamp& ts) {
    return os << ts.to_string();
}

} // namespace loglens
