#include <iostream>
#include <vector>
#include <format>
#include <random>

#include <fstream>
#include <variant>

#include "config_reader.h"

#include "sensor_types.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

void ReadConfig(
    std::vector<std::string>* sensors_ptr,
    std::vector<std::tuple<std::string, RuleType,
                           std::optional<std::pair<std::string, std::string>>>>* rules_ptr) {
    auto& sensors = *sensors_ptr;
    auto& rules = *rules_ptr;

    // Читаем JSON файл
    std::ifstream config_file("config.json");
    if (!config_file.is_open()) {
        throw std::runtime_error("Failed to open config.json: file not found or access denied.");
    }

    std::stringstream buffer;
    buffer << config_file.rdbuf();
    std::string content = buffer.str();
    config_file.close();
    // Исправляем кавычки
    std::string fixed_content = FixJsonQuotesSafe(content);
    json config = json::parse(fixed_content);

    // ===== Парсим sensors =====
    for (const auto& sensor : config["sensors"]) {
        std::string description = sensor["rule"];
        sensors.push_back(description);
    }

    // ===== Парсим rules =====
    for (const auto& rule : config["rules"]) {
        std::string name = rule["name"];
        std::string type = rule["type"];
        std::string pattern = rule["rule"];

        // Для правил типа bool могут быть дополнительные поля
        if (type == "bool" && rule.contains("true") && rule.contains("false")) {
            std::string true_val = rule["true"];
            std::string false_val = rule["false"];

            rules.emplace_back(pattern, RuleType::logical_bool,
                               std::make_pair(true_val, false_val));
        } else {
            rules.emplace_back(pattern, RuleTypeFromString(type), std::nullopt);
        }
    }
}

// Вспомогательная функция для замены первой найденной подстроки
void replace_first(std::string& s, const std::string& search, const std::string& replace) {
    size_t pos = s.find(search);
    if (pos != std::string::npos) {
        s.replace(pos, search.length(), replace);
    }
}

// Вспомогательная функция для форматирования чисел (чтобы не было лишних нулей)
std::string format_number(double val, int precision = 1) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << val;
    std::string result = oss.str();
    // Убираем ".0" если число целое
    if (result.find(".0") != std::string::npos) {
        result = result.substr(0, result.length() - 2);
    }
    return result;
}

int main() {
    int N = 4;

    std::vector<std::string> sensor_descriptions;
    std::vector<
        std::tuple<std::string, RuleType, std::optional<std::pair<std::string, std::string>>>>
        rules_descriptions;

    ReadConfig(&sensor_descriptions, &rules_descriptions);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis_value(0.0, 100.0);
    std::uniform_int_distribution<int> dis_bool(0, 1);
    const std::vector<std::string> speed_units = {"bit", "Kbit", "Mbit", "Gbit", "Tbit", "Pbit"};
    std::uniform_int_distribution<int> unit_dist(0, speed_units.size() - 1);

    for (int file_ind = 1; file_ind <= N; ++file_ind) {

        // Читаем JSON файл
        std::ofstream file(std::format("file{}.txt", file_ind));
        if (!file.is_open()) {
            throw std::runtime_error(
                std::format("Failed to open file: file{} not found or access denied.", file_ind));
        };

        for (std::string sensor_description : sensor_descriptions) {
            file << sensor_description << ":\n";
            for (const auto& [description, type, bool_tag] : rules_descriptions) {
                auto descr = description;
                if (type == RuleType::value) {
                    std::string val = format_number(dis_value(gen), 1);
                    replace_first(descr, "(.*)", val);

                } else if (type == RuleType::logical_bool) {
                    const auto& [true_val, false_val] = bool_tag.value();
                    std::string val = dis_bool(gen) ? true_val : false_val;
                    replace_first(descr, "(.*)", val);
                } else if (type == RuleType::speed) {
                    std::string val = format_number(dis_value(gen), 1);
                    std::string unit = speed_units[unit_dist(gen)];

                    replace_first(descr, "(.*)", val);
                    replace_first(descr, "(.*)", unit);
                }
                file << "    " << descr << "\n";
            }
        }
    }
    return 0;
}