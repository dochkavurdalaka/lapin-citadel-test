#pragma once

#include "sensor_types.h"

#include <concepts>

struct MultiThreadSensor {
private:
    std::string name_;
    std::string description_;
    std::vector<std::string> rules_;
    int thread_count_;

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
    std::unordered_map<std::string, std::vector<Measure<std::monostate, SpeedType, bool, float>>>
        measurements_;

public:
    MultiThreadSensor(const std::string& name, const std::string& description, int thread_count)
        : name_(name), description_(description), thread_count_(thread_count) {
    }

    void Add(const std::string& rule_name) {
        rules_.push_back(rule_name);
        auto& measure_vec = measurements_[rule_name];
        measure_vec.resize(thread_count_);
        std::fill(measure_vec.begin(), measure_vec.end(),
                  Measure<std::monostate, SpeedType, bool, float>{
                      .res = MinMaxPair<std::monostate>{}, .min_path = "", .max_path = ""});
    }

    Measure<std::monostate, SpeedType, bool, float>& GetMeasure(const std::string& rule_name,
                                                                int thread_ind) {
        return measurements_.at(rule_name)[thread_ind];
    }

    const Measure<std::monostate, SpeedType, bool, float>& GetMeasure(const std::string& rule_name,
                                                                      int thread_ind) const {
        return measurements_.at(rule_name)[thread_ind];
    }

    // "схлопывает" все измерения из разных потоков в измерение с индексом 0
    void Collapse() {
        for (auto& [rule, measure_vec] : measurements_) {
            auto& base_measure = measure_vec[0];
            for (size_t ind = 1; ind < measure_vec.size(); ++ind) {
                const auto& [res, min_path, max_path] = measure_vec[ind];
                std::visit(
                    [&](auto&& pair) {
                        using PairType = std::decay_t<decltype(pair)>;
                        std::string output;
                        if constexpr (std::same_as<PairType, MinMaxPair<SpeedType>>) {
                            base_measure.Update(pair.max, max_path);
                            base_measure.Update(pair.min, min_path);
                        } else if constexpr (std::same_as<PairType, MinMaxPair<float>>) {
                            base_measure.Update(pair.max, max_path);
                            base_measure.Update(pair.min, min_path);
                        } else if constexpr (std::same_as<PairType, MinMaxPair<bool>>) {
                            base_measure.Update(pair.max, max_path);
                            base_measure.Update(pair.min, min_path);
                        };
                    },
                    res);
            }
        }
    }
};