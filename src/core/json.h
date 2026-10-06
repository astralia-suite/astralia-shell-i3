#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace astralia {

struct Json {
    enum class Type { null,
                      boolean,
                      number,
                      string,
                      array,
                      object };

    struct Member;

    Type type = Type::null;
    bool boolean = false;
    double number = 0.0;
    std::string string;
    std::vector<Json> array;
    std::vector<Member> object;

    const Json *find(std::string_view key) const;
    double number_or(std::string_view key, double fallback) const;
    std::string string_or(std::string_view key, std::string_view fallback = {}) const;
    bool boolean_or(std::string_view key, bool fallback) const;
};

struct Json::Member {
    std::string key;
    Json value;
};

std::optional<Json> parse_json(std::string_view text);

} // namespace astralia
