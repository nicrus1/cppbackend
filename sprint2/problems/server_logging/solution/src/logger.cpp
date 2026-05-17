#include "logger.h"
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/support/date_time.hpp>
#include <iostream>

namespace logging {

void InitLogger() {
    // Добавляем общие атрибуты
    boost::log::add_common_attributes();
    
    // Настраиваем вывод в консоль
    auto sink = boost::log::add_console_log(std::cout);
    
    // Задаём форматтер для вывода JSON
    sink->set_formatter([](const boost::log::record_view& rec, 
                           boost::log::formatting_ostream& strm) {
        auto data = rec[additional_data];
        if (data) {
            strm << boost::json::serialize(data->as_object());
        }
    });
    
    // Устанавливаем уровень логирования
    boost::log::core::get()->set_filter(
        boost::log::trivial::severity >= boost::log::trivial::info
    );
}

std::string GetCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    auto now_ms = std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()) % 1000000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_time_t), "%Y-%m-%dT%H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(6) << now_ms.count();
    return ss.str();
}

} // namespace logging