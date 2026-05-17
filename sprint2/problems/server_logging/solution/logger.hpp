#pragma once

#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/utility/formatting_ostream.hpp>
#include <boost/log/utility/value_ref.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/json.hpp>

#include <iostream>

namespace logging = boost::log;
namespace json = boost::json;
namespace keywords = boost::log::keywords;

// ❗ переименовано, чтобы не конфликтовать с logging_handler.hpp
BOOST_LOG_ATTRIBUTE_KEYWORD(message_kw, "Message", std::string)
BOOST_LOG_ATTRIBUTE_KEYWORD(data_kw, "Data", json::value)
BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp_attr, "TimeStamp", boost::posix_time::ptime)

inline void InitLogger() {
    logging::add_common_attributes();

    logging::add_console_log(
        std::cout,
        keywords::format = [](logging::record_view const& rec,
                              logging::formatting_ostream& stream) {
            json::object obj;

            // timestamp
            auto ts = logging::extract<boost::posix_time::ptime>(timestamp_attr.name(), rec);
            if (ts)
                obj["timestamp"] = boost::posix_time::to_iso_extended_string(ts.get());
            else
                obj["timestamp"] = "";

            // message
            auto msg = logging::extract<std::string>(message_kw.name(), rec);
            obj["message"] = msg ? msg.get() : "";

            // data
            auto data = logging::extract<json::value>(data_kw.name(), rec);
            if (data)
                obj["data"] = data.get();
            else
                obj["data"] = json::object{};

            stream << json::serialize(obj);
        }
    );
}