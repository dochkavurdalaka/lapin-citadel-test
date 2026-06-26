#include <iostream>
#include <vector>
#include <format>

#include <variant>

#include "sensor_multithread.h"
#include "config_reader.h"
#include "log_processor_multithread.h"
#include "json.hpp"


using json = nlohmann::json;
namespace fs = std::filesystem;

std::vector<std::filesystem::path> FindFiles() {
    std::regex file_name_pattern(R"(file([0-9]*)\.txt)");
    fs::path current_path = fs::current_path();

    std::vector<fs::path> results;
    for (const auto& entry : fs::directory_iterator(current_path)) {
        if (entry.is_regular_file()) {
            std::string filename = entry.path().filename().string();
            if (std::regex_match(filename, file_name_pattern)) {
                results.push_back(entry.path());
            }
        }
    }

    return results;
}

void PrintResult(std::unordered_map<std::string, MultiThreadSensor>& sensors_map,
                 const std::unordered_map<std::string, Rule>& rule_map) {
    std::vector<std::string> keys;
    for (const auto& [key, value] : sensors_map) {
        keys.push_back(key);
    }
    std::sort(keys.begin(), keys.end());

    for (const std::string& key : keys) {
        auto& sensor = sensors_map.at(key);
        sensor.Collapse();
        std::cout << sensor.GetName() << "\n";
        std::vector<std::string> rules = sensor.GetRules();
        std::sort(rules.begin(), rules.end());

        for (const std::string& rule_name : rules) {
            const auto& measure = sensor.GetMeasure(rule_name, 0);
            const auto& [res, min_path, max_path] = measure;

            std::visit(
                [&](auto&& pair) {
                    using PairType = std::decay_t<decltype(pair)>;
                    std::string output;
                    if constexpr (std::same_as<PairType, MinMaxPair<std::monostate>>) {
                        output = std::format("{}: max=N/A(), min=N/A()", rule_name);
                    } else if constexpr (std::same_as<PairType, MinMaxPair<SpeedType>>) {
                        auto get_unit = [](const SpeedType& speed) -> std::string {
                            return (speed.prefix == '0') ? "bit"
                                                         : std::string(1, speed.prefix) + "bit";
                        };
                        SpeedType max = pair.max;
                        SpeedType min = pair.min;
                        output = std::format("{}: max={} {}/s ({}), min={} {}/s ({})", rule_name,
                                             max.value, get_unit(max), max_path, min.value,
                                             get_unit(min), min_path);
                    } else if constexpr (std::same_as<PairType, MinMaxPair<float>>) {
                        output = std::format("{}: max={} ({}), min={} ({})", rule_name, pair.max,
                                             max_path, pair.min, min_path);
                    } else if constexpr (std::same_as<PairType, MinMaxPair<bool>>) {
                        const auto& [true_val, false_val] = rule_map.at(rule_name).bool_tag.value();
                        std::string max = pair.max ? true_val : false_val;
                        std::string min = pair.min ? true_val : false_val;
                        output = std::format("{}: max={} ({}), min={} ({})", rule_name, max,
                                             max_path, min, min_path);
                    }
                    std::cout << "    " << output << "\n";
                },
                res);
        }
    }
}

int main(int argc, char* argv[]) {

    int threads_count = std::thread::hardware_concurrency();

    std::unordered_map<std::string, MultiThreadSensor> sensors_map;
    std::unordered_map<std::string, Rule> rules_map;

    std::string prs_config = ReadConfig(&sensors_map, &rules_map, threads_count);
    if (not prs_config.empty()) {
        std::cout << prs_config << std::endl;
        return 0;
    }

    LogProcessorMultiThread log_processor(&rules_map, &sensors_map, threads_count);


    std::vector<std::filesystem::path> paths;
    for (int i = 1; i < argc; ++i) {
        paths.emplace_back(argv[i]);
    }

    auto all_files = FindFiles();
    std::string log_string = log_processor.ProcessFiles(all_files);

    PrintResult(sensors_map, rules_map);
    std::cout << log_string;

    return 0;
}