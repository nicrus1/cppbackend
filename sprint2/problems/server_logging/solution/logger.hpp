#pragma once

#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/expressions.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/json.hpp>

namespace logging = boost::log;
namespace json = boost::json;
namespace expr = boost::log::expressions;

BOOST_LOG_ATTRIBUTE_KEYWORD(message_attr, "Message", std::string)
BOOST_LOG_ATTRIBUTE_KEYWORD(data_attr, "Data", json::value)

inline void InitLogger() {
    logging::add_common_attributes();

    logging::add_console_log(
        std::cout,
        logging::keywords::format = [](logging::record_view const& rec,
                                       logging::formatting_ostream& stream) {

            json::object obj;

            auto ts = rec[timestamp_attr];
            obj["timestamp"] = boost::posix_time::to_iso_extended_string(*ts);

            obj["message"] = rec[message_attr] ? rec[message_attr].get() : "";

            if (rec[data_attr])
                obj["data"] = rec[data_attr].get();
            else
                obj["data"] = json::object{};

            stream << json::serialize(obj);
        }
    );
}