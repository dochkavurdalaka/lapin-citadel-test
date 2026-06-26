#pragma once

#include <iostream>
#include <fstream>

#include "sensor_types.h"
#include "sensor_multithread.h"
#include "json.hpp"

// функция, которая ординарне кавычки в .json файле заменяет на двойные
std::string FixJsonQuotesSafe(const std::string& input) {
    std::string result;
    bool in_string = false;
    char opening_quote = 0;
    
    for (char ch : input) {
        if (!in_string && (ch == '"' || ch == '\'')) {
            // Начало строки
            in_string = true;
            opening_quote = ch;
            result += '"';  // Всегда записываем двойную кавычку
        }
        else if (in_string && ch == opening_quote) {
            // Конец строки
            in_string = false;
            opening_quote = 0;
            result += '"';  // Всегда записываем двойную кавычку
        }
        else {
            result += ch;
        }
    }
    return result;
}

void ReadConfigCore(std::unordered_map<std::string, Sensor>* sensors_map_ptr,
                    std::unordered_map<std::string, Rule>* rules_map_ptr) {
    auto& sensors_map = *sensors_map_ptr;
    auto& rules_map = *rules_map_ptr;

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

    // // ===== Парсим sensors =====
    // for (const auto& sensor : config["sensors"]) {
    //     std::string name = sensor["name"];
    //     std::string description = sensor["rule"];
    //     sensors_map.emplace(name, Sensor(name, description));
    // }

    // // ===== Парсим rules =====
    // for (const auto& rule : config["rules"]) {
    //     std::string name = rule["name"];
    //     std::string type = rule["type"];
    //     std::string pattern = rule["rule"];

    //     // Для правил типа bool могут быть дополнительные поля
    //     if (type == "bool" && rule.contains("true") && rule.contains("false")) {
    //         std::string true_val = rule["true"];
    //         std::string false_val = rule["false"];

    //         rules_map.emplace(name,
    //                           Rule(name, RuleType::logical_bool, pattern, true_val, false_val));
    //     } else {
    //         rules_map.emplace(name, Rule(name, RuleTypeFromString(type), pattern));
    //     }
    // }

    // // ===== Парсим extractors =====
    // for (const auto& extractor : config["extractors"]) {
    //     std::string sensor = extractor["sensor"];
    //     std::vector<std::string> rules = extractor["rules"].get<std::vector<std::string>>();

    //     for (const auto& rule : rules) {
    //         sensors_map.at(sensor).Add(rules_map.at(rule).name);
    //     }
    // }

    // ===== Парсим sensors =====
    if (!config.contains("sensors")) {
        throw std::runtime_error("Missing required key: 'sensors' in config.json");
    }

    if (!config["sensors"].is_array()) {
        throw std::runtime_error("'sensors' must be an array in config.json");
    }

    for (const auto& sensor : config["sensors"]) {
        if (!sensor.contains("name")) {
            throw std::runtime_error("Sensor missing required field 'name'");
        }
        if (!sensor.contains("rule")) {
            throw std::runtime_error("Sensor '" + sensor.at("name").get<std::string>() +
                                     "' missing required field 'rule'");
        }
        std::string name = sensor["name"];
        std::string description = sensor["rule"];
        sensors_map.emplace(name, Sensor(name, description));
    }

    // ===== Парсим rules =====
    if (!config.contains("rules")) {
        throw std::runtime_error("Missing required key: 'rules' in config.json");
    }

    if (!config["rules"].is_array()) {
        throw std::runtime_error("'rules' must be an array in config.json");
    }

    for (const auto& rule : config["rules"]) {
        if (!rule.contains("name")) {
            throw std::runtime_error("Rule missing required field 'name'");
        }
        if (!rule.contains("type")) {
            throw std::runtime_error("Rule missing required field 'type'");
        }
        if (!rule.contains("rule")) {
            throw std::runtime_error("Rule '" + rule.at("name").get<std::string>() +
                                     "' missing required field 'rule'");
        }

        std::string name = rule["name"];
        std::string type = rule["type"];
        std::string pattern = rule["rule"];

        // Для правил типа bool могут быть дополнительные поля
        if (type == "bool" && rule.contains("true") && rule.contains("false")) {

            if (!rule.contains("true") || !rule.contains("false")) {
                throw std::runtime_error("Bool rule '" + name +
                                         "' missing required fields 'true' and/or 'false'");
            }

            std::string true_val = rule["true"];
            std::string false_val = rule["false"];

            rules_map.emplace(name,
                              Rule(name, RuleType::logical_bool, pattern, true_val, false_val));
        } else {
            rules_map.emplace(name, Rule(name, RuleTypeFromString(type), pattern));
        }
    }

    // ===== Парсим extractors =====

        if (!config.contains("extractors")) {
        throw std::runtime_error("Missing required key: 'extractors' in config.json");
    }
    
    if (!config["extractors"].is_array()) {
        throw std::runtime_error("'extractors' must be an array in config.json");
    }

    for (const auto& extractor : config["extractors"]) {

        if (!extractor.contains("sensor")) {
            throw std::runtime_error("Extractor missing required field 'sensor'");
        }
        if (!extractor.contains("rules")) {
            throw std::runtime_error("Extractor missing required field 'rules'");
        }
        if (!extractor["rules"].is_array()) {
            throw std::runtime_error("Extractor 'rules' must be an array");
        }
        
        std::string sensor = extractor["sensor"];
        std::vector<std::string> rules = extractor["rules"].get<std::vector<std::string>>();

        for (const auto& rule : rules) {
            sensors_map.at(sensor).Add(rules_map.at(rule).name);
        }
    }
}

std::string ReadConfig(std::unordered_map<std::string, Sensor>* sensors_map_ptr,
                       std::unordered_map<std::string, Rule>* rules_map_ptr) {
    try {
        ReadConfigCore(sensors_map_ptr, rules_map_ptr);
        return "";  // Возвращаем пустую строку в случае успеха (ошибок нет)
    } catch (const nlohmann::json::parse_error& e) {
        return "Ошибка синтаксиса JSON: " + std::string(e.what()) +
               "\n(ID ошибки: " + std::to_string(e.id) + ", символ: " + std::to_string(e.byte) +
               ")";
    } catch (const nlohmann::json::type_error& e) {
        return "Ошибка несоответствия типов в JSON: " + std::string(e.what()) +
               "\nПроверьте наличие и типы обязательных полей.";
    } catch (const std::regex_error& e) {
        return "Некорректное регулярное выражение в правилах: " + std::string(e.what()) +
               "\nКод ошибки: " + std::to_string(static_cast<int>(e.code()));
    } catch (const std::invalid_argument& e) {
        return "Недопустимый аргумент: " + std::string(e.what());
    } catch (const std::out_of_range& e) {
        return "Ошибка связывания: " + std::string(e.what()) +
               "\nПроверьте, что все правила и сенсоры из 'extractors' объявлены.";
    } catch (const std::runtime_error& e) {
        return "Ошибка времени выполнения: " + std::string(e.what());
    } catch (const std::exception& e) {
        return "Непредвиденная ошибка при чтении конфигурации: " + std::string(e.what());
    }
}



void ReadConfigCore(std::unordered_map<std::string, MultiThreadSensor>* sensors_map_ptr,
                    std::unordered_map<std::string, Rule>* rules_map_ptr, int threads_count) {
    auto& sensors_map = *sensors_map_ptr;
    auto& rules_map = *rules_map_ptr;

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

    // // ===== Парсим sensors =====
    // for (const auto& sensor : config["sensors"]) {
    //     std::string name = sensor["name"];
    //     std::string description = sensor["rule"];
    //     sensors_map.emplace(name, MultiThreadSensor(name, description, threads_count));
    // }

    // // ===== Парсим rules =====
    // for (const auto& rule : config["rules"]) {
    //     std::string name = rule["name"];
    //     std::string type = rule["type"];
    //     std::string pattern = rule["rule"];

    //     // Для правил типа bool могут быть дополнительные поля
    //     if (type == "bool" && rule.contains("true") && rule.contains("false")) {
    //         std::string true_val = rule["true"];
    //         std::string false_val = rule["false"];

    //         rules_map.emplace(name,
    //                           Rule(name, RuleType::logical_bool, pattern, true_val, false_val));
    //     } else {
    //         rules_map.emplace(name, Rule(name, RuleTypeFromString(type), pattern));
    //     }
    // }

    // // ===== Парсим extractors =====
    // for (const auto& extractor : config["extractors"]) {
    //     std::string sensor = extractor["sensor"];
    //     std::vector<std::string> rules = extractor["rules"].get<std::vector<std::string>>();

    //     for (const auto& rule : rules) {
    //         sensors_map.at(sensor).Add(rules_map.at(rule).name);
    //     }
    // }

    // ===== Парсим sensors =====
    if (!config.contains("sensors")) {
        throw std::runtime_error("Missing required key: 'sensors' in config.json");
    }

    if (!config["sensors"].is_array()) {
        throw std::runtime_error("'sensors' must be an array in config.json");
    }

    for (const auto& sensor : config["sensors"]) {
        if (!sensor.contains("name")) {
            throw std::runtime_error("Sensor missing required field 'name'");
        }
        if (!sensor.contains("rule")) {
            throw std::runtime_error("Sensor '" + sensor.at("name").get<std::string>() +
                                     "' missing required field 'rule'");
        }
        std::string name = sensor["name"];
        std::string description = sensor["rule"];
        sensors_map.emplace(name, MultiThreadSensor(name, description, threads_count));
    }

    // ===== Парсим rules =====
    if (!config.contains("rules")) {
        throw std::runtime_error("Missing required key: 'rules' in config.json");
    }

    if (!config["rules"].is_array()) {
        throw std::runtime_error("'rules' must be an array in config.json");
    }

    for (const auto& rule : config["rules"]) {
        if (!rule.contains("name")) {
            throw std::runtime_error("Rule missing required field 'name'");
        }
        if (!rule.contains("type")) {
            throw std::runtime_error("Rule missing required field 'type'");
        }
        if (!rule.contains("rule")) {
            throw std::runtime_error("Rule '" + rule.at("name").get<std::string>() +
                                     "' missing required field 'rule'");
        }

        std::string name = rule["name"];
        std::string type = rule["type"];
        std::string pattern = rule["rule"];

        // Для правил типа bool могут быть дополнительные поля
        if (type == "bool" && rule.contains("true") && rule.contains("false")) {

            if (!rule.contains("true") || !rule.contains("false")) {
                throw std::runtime_error("Bool rule '" + name +
                                         "' missing required fields 'true' and/or 'false'");
            }

            std::string true_val = rule["true"];
            std::string false_val = rule["false"];

            rules_map.emplace(name,
                              Rule(name, RuleType::logical_bool, pattern, true_val, false_val));
        } else {
            rules_map.emplace(name, Rule(name, RuleTypeFromString(type), pattern));
        }
    }

    // ===== Парсим extractors =====

        if (!config.contains("extractors")) {
        throw std::runtime_error("Missing required key: 'extractors' in config.json");
    }
    
    if (!config["extractors"].is_array()) {
        throw std::runtime_error("'extractors' must be an array in config.json");
    }

    for (const auto& extractor : config["extractors"]) {

        if (!extractor.contains("sensor")) {
            throw std::runtime_error("Extractor missing required field 'sensor'");
        }
        if (!extractor.contains("rules")) {
            throw std::runtime_error("Extractor missing required field 'rules'");
        }
        if (!extractor["rules"].is_array()) {
            throw std::runtime_error("Extractor 'rules' must be an array");
        }
        
        std::string sensor = extractor["sensor"];
        std::vector<std::string> rules = extractor["rules"].get<std::vector<std::string>>();

        for (const auto& rule : rules) {
            sensors_map.at(sensor).Add(rules_map.at(rule).name);
        }
    }
}

std::string ReadConfig(std::unordered_map<std::string, MultiThreadSensor>* sensors_map_ptr,
                       std::unordered_map<std::string, Rule>* rules_map_ptr, int threads_count) {
    try {
        ReadConfigCore(sensors_map_ptr, rules_map_ptr, threads_count);
        return "";  // Возвращаем пустую строку в случае успеха (ошибок нет)
    } catch (const nlohmann::json::parse_error& e) {
        return "Ошибка синтаксиса JSON: " + std::string(e.what()) +
               "\n(ID ошибки: " + std::to_string(e.id) + ", символ: " + std::to_string(e.byte) +
               ")";
    } catch (const nlohmann::json::type_error& e) {
        return "Ошибка несоответствия типов в JSON: " + std::string(e.what()) +
               "\nПроверьте наличие и типы обязательных полей.";
    } catch (const std::regex_error& e) {
        return "Некорректное регулярное выражение в правилах: " + std::string(e.what()) +
               "\nКод ошибки: " + std::to_string(static_cast<int>(e.code()));
    } catch (const std::invalid_argument& e) {
        return "Недопустимый аргумент: " + std::string(e.what());
    } catch (const std::out_of_range& e) {
        return "Ошибка связывания: " + std::string(e.what()) +
               "\nПроверьте, что все правила и сенсоры из 'extractors' объявлены.";
    } catch (const std::runtime_error& e) {
        return "Ошибка времени выполнения: " + std::string(e.what());
    } catch (const std::exception& e) {
        return "Непредвиденная ошибка при чтении конфигурации: " + std::string(e.what());
    }
}