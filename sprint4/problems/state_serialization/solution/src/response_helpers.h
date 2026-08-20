#pragma once
#include <boost/beast/http.hpp>
#include <boost/json.hpp>

namespace http_handler {
namespace response_helpers {

namespace http = boost::beast::http;

template <typename Send>
void SendErrorResponse(Send&& send, http::status status,
                       const std::string& code, const std::string& message) {
  boost::json::object response_obj;
  response_obj["code"] = code;
  response_obj["message"] = message;

  http::response<http::string_body> response{status, 11};
  response.set(http::field::content_type, "application/json");
  response.set(http::field::cache_control, "no-cache");
  response.body() = boost::json::serialize(response_obj);
  response.prepare_payload();
  send(std::move(response));
}

template <typename Send>
void SendErrorResponseWithAllow(Send&& send, http::status status,
                                const std::string& code,
                                const std::string& message,
                                const std::string& allow_methods) {
  boost::json::object response_obj;
  response_obj["code"] = code;
  response_obj["message"] = message;

  http::response<http::string_body> response{status, 11};
  response.set(http::field::content_type, "application/json");
  response.set(http::field::allow, allow_methods);
  response.set(http::field::cache_control, "no-cache");
  response.body() = boost::json::serialize(response_obj);
  response.prepare_payload();
  send(std::move(response));
}

template <typename Send>
void SendTextErrorResponse(Send&& send, http::status status,
                           const std::string& message) {
  http::response<http::string_body> response{status, 11};
  response.set(http::field::content_type, "text/plain");
  response.body() = message;
  response.prepare_payload();
  send(std::move(response));
}

template <typename Send>
void SendSuccessResponse(Send&& send, const std::string& body) {
  http::response<http::string_body> response{http::status::ok, 11};
  response.set(http::field::content_type, "application/json");
  response.body() = body;
  response.prepare_payload();
  send(std::move(response));
}

}  // namespace response_helpers
}  // namespace http_handler