#pragma once

#include <vector>
#include <unordered_map>
#include <string>
#include <regex>
#include <variant>

#include "json.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

enum class RuleType {
    logical_bool,  // bool
    value,         // int
    speed          // float
};

struct Rule {
    std::string name;
    RuleType type;
    std::regex pattern;

    // используются, если у нас тип bool
    std::optional<std::pair<std::string, std::string>> bool_tag;

    Rule(const std::string& name, RuleType type, const std::string& pattern_str)
        : name(name), type(type), pattern(pattern_str), bool_tag{std::nullopt} {
    }

    Rule(const std::string& name, RuleType type, const std::string& pattern_str,
         const std::string& true_val, const std::string& false_val)
        : name(name),
          type(type),
          pattern(pattern_str),
          bool_tag{std::make_pair(true_val, false_val)} {
    }
};

struct SpeedType {
    static inline std::array<int, 128> prefix_table = []() {
        std::array<int, 128> arr{};
        arr.fill(-1);  // по умолчанию 0
        arr['0'] = 0;
        arr['K'] = 1;
        arr['M'] = 2;
        arr['G'] = 3;
        arr['T'] = 4;
        arr['P'] = 5;
        return arr;
    }();

    static inline std::array<int64_t, 6> multipliers = {
        1,                   // 0 - 10⁰
        1000,                // 1 - 10³ (K)
        1000000,             // 2 - 10⁶ (M)
        1000000000,          // 3 - 10⁹ (G)
        1000000000000LL,     // 4 - 10¹² (T)
        1000000000000000LL,  // 5 - 10¹⁵ (P)
    };

    double value;
    char prefix;

    int64_t GetScaled() const {
        int idx = (prefix >= 0) ? prefix_table[prefix] : 0;
        return std::round(value * multipliers[idx]);
    }
};

// bool operator<(const SpeedType& one, const SpeedType& two) {
//     if (SpeedType::prefix_table[one.prefix] < SpeedType::prefix_table[two.prefix]) {
//         if (one.value < two.value) {
//             return true;
//         }
//         int64_t multiplier = SpeedType::multipliers[SpeedType::prefix_table[two.prefix] -
//         SpeedType::prefix_table[one.prefix]]; return one.value < two.value * multiplier;
//     } else if (SpeedType::prefix_table[one.prefix] == SpeedType::prefix_table[two.prefix]) {
//         return one.value < two.value;
//     } else {
//         if (one.value > two.value) {
//             return false;
//         }
//         int64_t multiplier = SpeedType::multipliers[SpeedType::prefix_table[one.prefix] -
//         SpeedType::prefix_table[two.prefix]]; return one.value * multiplier < two.value;
//     }
// }

bool operator<(const SpeedType& one, const SpeedType& two) {
    return one.GetScaled() < two.GetScaled();
}

template <class T>
struct MinMaxPair {
    T min;
    T max;
};

template <class... Args>
struct Measure {
    std::variant<MinMaxPair<Args>...> res;
    std::string min_path;
    std::string max_path;

    template <class T>
    void Update(const T& recv_value, const std::filesystem::path& file_path) {
        // если измерение не пустое
        if (not std::holds_alternative<MinMaxPair<std::monostate>>(res)) {
            auto& pair = std::get<MinMaxPair<T>>(res);
            if (recv_value < pair.min) {
                pair.min = recv_value;
                min_path = file_path;
            }
            if (pair.max < recv_value) {
                pair.max = recv_value;
                max_path = file_path;
            }
        } else {
            res = MinMaxPair{recv_value, recv_value};
            min_path = file_path;
            max_path = file_path;
        }
    }
};

struct Sensor {
private:
    std::string name_;
    std::string description_;
    std::vector<std::string> rules_;

public:
    const std::string& GetName() const {
        return name_;
    }

    const std::string& GetDescription() const {
        return description_;
    }

    const std::vector<std::string>& GetRules() const {
        return rules_;
    }

private:
    // monostate - измерение пустое
    std::unordered_map<std::string, Measure<std::monostate, SpeedType, bool, float>> measurements_;

public:
    Sensor(const std::string& name, const std::string& description)
        : name_(name), description_(description) {
    }

    void Add(const std::string& rule_name) {
        rules_.push_back(rule_name);
        measurements_[rule_name] = Measure<std::monostate, SpeedType, bool, float>{
            .res = MinMaxPair<std::monostate>{}, .min_path = "", .max_path = ""};
    }

    Measure<std::monostate, SpeedType, bool, float>& GetMeasure(const std::string& rule_name) {
        return measurements_.at(rule_name);
    }

    const Measure<std::monostate, SpeedType, bool, float>& GetMeasure(
        const std::string& rule_name) const {
        return measurements_.at(rule_name);
    }
};

RuleType RuleTypeFromString(const std::string& str) {
    if (str == "logical_bool")
        return RuleType::logical_bool;
    if (str == "value")
        return RuleType::value;
    if (str == "speed")
        return RuleType::speed;

    throw std::invalid_argument("Unknown RuleType: " + str);
}