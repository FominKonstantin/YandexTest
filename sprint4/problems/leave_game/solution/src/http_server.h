// http_server.h
#pragma once
#include "sdk.h"
#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <functional>
#include <memory>
#include <string>
#include <variant>

#include "logging_request_handler.h"

namespace http_server {

namespace net = boost::asio;
using tcp = net::ip::tcp;
namespace beast = boost::beast;
namespace http = beast::http;

using ResponseVariant = std::variant<http::response<http::string_body>,
                                     http::response<http::file_body>>;

class SessionBase {
public:
  explicit SessionBase(tcp::socket &&socket) : stream_(std::move(socket)) {}
  virtual ~SessionBase() = default;
  void Run() { ReadRequest(); }

protected:
  virtual void ReadRequest() = 0;
  virtual void OnReadRequest(beast::error_code ec,
                             size_t bytes_transferred) = 0;
  virtual void WriteResponse() = 0;
  virtual void OnWriteResponse(beast::error_code ec,
                               size_t bytes_transferred) = 0;

  beast::tcp_stream stream_;
  beast::flat_buffer buffer_;
  http::request<http::string_body> request_;
  ResponseVariant response_;
  std::string client_ip_ = "unknown";
};

template <typename RequestHandler>
class Session : public SessionBase,
                public std::enable_shared_from_this<Session<RequestHandler>> {
public:
  template <typename Handler>
  Session(tcp::socket &&socket, Handler &&handler)
      : SessionBase(std::move(socket)),
        handler_(std::forward<Handler>(handler)) {}

  void SetClientIp(const std::string &ip) { client_ip_ = ip; }

protected:
  void ReadRequest() override {
    auto self = this->shared_from_this();
    http::async_read(stream_, buffer_, request_,
                     [self](beast::error_code ec, size_t bytes_transferred) {
                       self->OnReadRequest(ec, bytes_transferred);
                     });
  }

  void OnReadRequest(beast::error_code ec, size_t) override {
    if (ec == http::error::end_of_stream) {
      stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
      return;
    }

    if (ec) {
      logging_handler::LogError(ec.value(), ec.message(), "read");
      return;
    }

    handler_(std::move(request_), client_ip_, [this](auto &&response) {
      response_ = std::forward<decltype(response)>(response);
      WriteResponse();
    });
  }

  void WriteResponse() override {
    auto self = this->shared_from_this();

    std::visit(
        [this, &self](auto &resp) {
          resp.prepare_payload();
          http::async_write(
              stream_, resp,
              [self](beast::error_code ec, size_t bytes_transferred) {
                self->OnWriteResponse(ec, bytes_transferred);
              });
        },
        response_);
  }

  void OnWriteResponse(beast::error_code ec, size_t) override {
    if (ec) {
      logging_handler::LogError(ec.value(), ec.message(), "write");
      return;
    }

    bool keep_alive =
        std::visit([](auto &resp) { return resp.keep_alive(); }, response_);

    if (!keep_alive) {
      stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
      return;
    }

    ReadRequest();
  }

private:
  RequestHandler handler_;
};

template <typename RequestHandler>
class Listener : public std::enable_shared_from_this<Listener<RequestHandler>> {
public:
  template <typename Handler>
  Listener(net::io_context &ioc, const tcp::endpoint &endpoint,
           Handler &&handler)
      : ioc_(ioc), acceptor_(ioc, endpoint),
        handler_(std::forward<Handler>(handler)) {}

  void Run() { Accept(); }

private:
  void Accept() {
    auto self = this->shared_from_this();
    acceptor_.async_accept([self](beast::error_code ec, tcp::socket socket) {
      self->OnAccept(ec, std::move(socket));
    });
  }

  void OnAccept(beast::error_code ec, tcp::socket socket) {
    if (ec) {
      logging_handler::LogError(ec.value(), ec.message(), "accept");
      Accept();
      return;
    }

    std::string client_ip = "unknown";
    try {
      client_ip = socket.remote_endpoint().address().to_string();
    } catch (const boost::system::system_error &e) {
      logging_handler::LogError(e.code().value(), e.what(),
                                "get_remote_endpoint");
    }

    auto session =
        std::make_shared<Session<RequestHandler>>(std::move(socket), handler_);
    session->SetClientIp(client_ip);
    session->Run();

    Accept();
  }

  net::io_context &ioc_;
  tcp::acceptor acceptor_;
  RequestHandler handler_;
};

template <typename RequestHandler>
void ServeHttp(net::io_context &ioc, const tcp::endpoint &endpoint,
               RequestHandler &&handler) {
  auto listener = std::make_shared<Listener<RequestHandler>>(
      ioc, endpoint, std::forward<RequestHandler>(handler));
  listener->Run();
}

} // namespace http_server