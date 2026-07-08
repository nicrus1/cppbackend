#include "htmldecode.h"

#include <string>
#include <unordered_map>
#include <cctype>
#include <algorithm>

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

            while (entity_end < str.size() && std::isalpha(str[entity_end])) {
                ++entity_end;
            }

            if (entity_end > entity_start) {
                std::string entity = std::string(str.substr(entity_start, entity_end - entity_start));
                std::string lower_entity = entity;
                std::transform(lower_entity.begin(), lower_entity.end(), 
                              lower_entity.begin(), ::tolower);

                auto it = entities.find(lower_entity);
                if (it != entities.end()) {
                    if (entity_end < str.size() && str[entity_end] == ';') {
                        ++entity_end;
                    }
                    
                    result.push_back(it->second);
                    i = entity_end - 1;
                    continue;
                }
            }

            result.push_back('&');
        } else {
            result.push_back(str[i]);
        }
    }

    return result;
}