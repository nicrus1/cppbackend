#include "htmldecode.h"

#include <string>
#include <unordered_map>
#include <cctype>

std::string HtmlDecode(std::string_view str) {
    static const std::unordered_map<std::string, char> entities = {
        {"lt", '<'},
        {"gt", '>'},
        {"amp", '&'},
        {"apos", '\''},
        {"quot", '\"'}
    };

    std::string result;
    result.reserve(str.size());

    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '&') {
            size_t entity_start = i + 1;
            size_t entity_end = entity_start;

            // Ищем конец мнемоники (буквы)
            while (entity_end < str.size() && std::isalpha(str[entity_end])) {
                ++entity_end;
            }

            // Если нашли хотя бы одну букву
            if (entity_end > entity_start) {
                std::string entity = std::string(str.substr(entity_start, entity_end - entity_start));
                std::string lower_entity = entity;
                for (char& c : lower_entity) {
                    c = std::tolower(c);
                }

                auto it = entities.find(lower_entity);
                if (it != entities.end()) {
                    // Проверяем, есть ли ';' после мнемоники (опционально)
                    if (entity_end < str.size() && str[entity_end] == ';') {
                        ++entity_end; // Пропускаем ';'
                    }
                    
                    result.push_back(it->second);
                    i = entity_end - 1; // Перемещаем указатель
                    continue;
                }
            }

            // Если не нашли мнемонику, просто добавляем '&'
            result.push_back('&');
        } else {
            result.push_back(str[i]);
        }
    }

    return result;
}