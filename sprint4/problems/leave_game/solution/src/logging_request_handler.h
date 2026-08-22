#pragma once

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/json.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <chrono>
#include <string>

namespace logging_handler {

namespace json = boost::json;
namespace logging = boost::log;
namespace expr = boost::log::expressions;
namespace keywords = boost::log::keywords;
namespace posix_time = boost::posix_time;

BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", json::value)
BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", posix_time::ptime)

inline void InitLogging() {
  logging::add_common_attributes();

  auto console_sink =
      logging::add_console_log(std::cout, keywords::auto_flush = true);

  console_sink->set_formatter(
      [](const logging::record_view &rec, logging::formatting_ostream &os) {
        json::object log_entry;

        auto ts = rec[timestamp];
        if (ts) {
          log_entry["timestamp"] = posix_time::to_iso_extended_string(*ts);
        }

        auto message = rec[expr::smessage];
        if (message) {
          log_entry["message"] = std::string(*message);
        }

        auto data = rec[additional_data];
        if (data) {
          log_entry["data"] = data.get();
        }

        os << json::serialize(log_entry);
      });
}

inline void LogError(int code, const std::string &text,
                     const std::string &where) {
  json::value data =
      json::object{{"code", code}, {"text", text}, {"where", where}};
  BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, data)
                          << "error";
}

inline void LogRequest(const std::string &ip, const std::string &uri,
                       const std::string &method) {
  json::value data = json::object{{"ip", ip}, {"URI", uri}, {"method", method}};
  BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, data)
                          << "request received";
}

inline void LogResponse(const std::string &ip,
                        std::chrono::milliseconds response_time,
                        int status_code, const std::string &content_type) {
  json::object data_obj;
  data_obj["ip"] = ip;
  data_obj["response_time"] = static_cast<std::int64_t>(response_time.count());
  data_obj["code"] = status_code;

  if (!content_type.empty()) {
    data_obj["content_type"] = content_type;
  } else {
    data_obj["content_type"] = nullptr;
  }

  json::value data = data_obj;
  BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, data)
                          << "response sent";
}

inline void LogServerStart(const std::string &address, unsigned short port) {
  json::value data = json::object{{"address", address}, {"port", port}};
  BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, data)
                          << "server started";
}

inline void LogServerExit(int code, const std::string &exception = "") {
  json::object data_obj;
  data_obj["code"] = code;
  if (!exception.empty()) {
    data_obj["exception"] = exception;
  }
  json::value data = data_obj;
  BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, data)
                          << "server exited";
}

} // namespace logging_handler