#pragma once

#include <unordered_set>
#include <fstream>
#include <format>

#include "sensor_types.h"

class LogProcessor {
    const std::unordered_map<std::string, Rule>& rules_map_;
    std::unordered_map<std::string, Sensor>& sensors_map_;
    std::unordered_map<std::string, std::string> desc_to_name_map_;

public:
    LogProcessor(const std::unordered_map<std::string, Rule>* rules,
                 std::unordered_map<std::string, Sensor>* sensors)
        : rules_map_(*rules), sensors_map_(*sensors) {
        for (const auto& [key, value] : sensors_map_) {
            desc_to_name_map_.emplace(value.GetDescription(), value.GetName());
        }
    }

    void ProcessFiles(const std::vector<fs::path>& file_pathes, std::string* log_parsing) {
        for (const auto& path : file_pathes) {
            ProcessFile(path, log_parsing);
        }
    }

    void ProcessFile(const fs::path& file_path, std::string* log_parsing) {
        std::ifstream file(file_path);
        if (!file.is_open()) {
            *log_parsing += std::format("Failed to open {}: file not found or access denied.\n",
                                        file_path.string());
            return;
        }

        std::string line;
        std::string cur_sensor_name;
        std::unordered_set<std::string> rules_to_find;

        while (std::getline(file, line)) {
            std::string trimmed_line = TrimSpacesAndTabs(line);
            if (trimmed_line.empty() or trimmed_line[0] == '/') {
                continue;
            }

            if (trimmed_line.back() == ':') {
                if (not cur_sensor_name.empty()) {
                    if (not rules_to_find.empty()) {
                        std::string rules_list;
                        for (const auto& rule_name : rules_to_find) {
                            if (!rules_list.empty())
                                rules_list += ", ";
                            rules_list += rule_name;
                        }
                        *log_parsing += std::format(
                            "There is no found rules in sensor: {}. Rules: {{{}}} in file: {}.\n",
                            cur_sensor_name, rules_list, file_path.string());
                    }
                }

                // Смена контекста сенсора
                cur_sensor_name.clear();
                rules_to_find.clear();
                std::string cur_sensor_descr = trimmed_line;
                cur_sensor_descr.pop_back();

                auto it = desc_to_name_map_.find(cur_sensor_descr);
                if (it != desc_to_name_map_.end()) {
                    cur_sensor_name = it->second;
                    for (const auto& rule : sensors_map_.at(cur_sensor_name).GetRules()) {
                        rules_to_find.insert(rule);
                    }
                } else {
                    *log_parsing +=
                        std::format("There is no exist sensor with description {}, file: {}\n",
                                    cur_sensor_descr, file_path.string());
                }
            } else if (not cur_sensor_name.empty()) {
                // Поиск правил в текущем контексте
                for (auto it = rules_to_find.begin(); it != rules_to_find.end();) {
                    const auto& rule = rules_map_.at(*it);
                    std::smatch matches;

                    if (std::regex_match(trimmed_line, matches, rule.pattern)) {
                        auto& sensor = sensors_map_.at(cur_sensor_name);
                        auto& measure = sensor.GetMeasure(rule.name);

                        // Парсинг в зависимости от типа правила
                        ParseAndStore(measure, rule, matches, file_path, cur_sensor_name,
                                      log_parsing);

                        it = rules_to_find.erase(it);  // Удаляем найденное правило
                        break;                         // выходим из цикла
                    } else {
                        ++it;
                    }
                }
            }
        }

        if (not cur_sensor_name.empty()) {
            if (not rules_to_find.empty()) {
                std::string rules_list;
                for (const auto& rule_name : rules_to_find) {
                    if (!rules_list.empty())
                        rules_list += ", ";
                    rules_list += rule_name;
                }
                *log_parsing += std::format(
                    "There is no found rules in sensor: {}. Rules: {{{}}} in file: {}.\n",
                    cur_sensor_name, rules_list, file_path.string());
            }
        }
    }

private:
    // Только пробелы и табы
    std::string TrimSpacesAndTabs(const std::string& str) {
        size_t start = str.find_first_not_of(" \t");

        if (start == std::string::npos) {
            return "";
        }

        size_t end = str.find_last_not_of(" \t");

        return str.substr(start, end - start + 1);
    }

    template <class... Args>
    void ParseAndStore(Measure<Args...>& measure, const Rule& rule,
                       const std::smatch& matches, const fs::path& file_path,
                       const std::string& cur_sensor_name, std::string* log_parsing) {
        if (rule.type == RuleType::speed) {
            SpeedType speed;
            try {
                speed.value = std::stod(matches[1].str());
            } catch (const std::exception& ex) {
                *log_parsing += std::format(
                    "Failed getting speed value from {{sensor: \"{}\", rule: \"{}\"}} in file: "
                    "{}\n",
                    cur_sensor_name, rule.name, file_path.string());
                return;
            }
            speed.prefix = (matches[2].str() == "bit") ? '0' : matches[2].str()[0];

            if (static_cast<size_t>(speed.prefix) >= speed.prefix_table.size() or
                speed.prefix_table[speed.prefix] == -1) {
                *log_parsing += std::format(
                    "Failed getting speed prefix from {{sensor: \"{}\", rule: \"{}\"}} in file: "
                    "{}\n",
                    cur_sensor_name, rule.name, file_path.string());
                return;
            }

            measure.Update(speed, file_path);

        } else if (rule.type == RuleType::logical_bool) {
            const auto& [true_val, false_val] = rule.bool_tag.value();
            if (matches[1].str() != true_val and matches[1].str() != false_val) {
                *log_parsing += std::format(
                    "Failed getting bool value from {{sensor: \"{}\", rule: \"{}\"}} in file: "
                    "{}\n",
                    cur_sensor_name, rule.name, file_path.string());
                return;
            }
            bool recv_value = (matches[1].str() == true_val);
            measure.Update(recv_value, file_path);
        } else if (rule.type == RuleType::value) {
            float recv_value;
            try {
                recv_value = std::stof(matches[1].str());
            } catch (const std::exception& ex) {
                *log_parsing += std::format(
                    "Failed getting float value from {{sensor: \"{}\", rule: \"{}\"}} in file: "
                    "{}\n",
                    cur_sensor_name, rule.name, file_path.string());
                return;
            }
            measure.Update(recv_value, file_path);
        }
    }
};