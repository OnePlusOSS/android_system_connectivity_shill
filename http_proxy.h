// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_HTTP_PROXY_H_
#define SHILL_HTTP_PROXY_H_

#include <string>
#include <vector>

#include <base/cancelable_callback.h>
#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>
#include <base/memory/weak_ptr.h>

#include "shill/byte_string.h"
#include "shill/refptr_types.h"

namespace shill {

class AsyncConnection;
class DNSClient;
class Error;
class EventDispatcher;
struct InputData;
class IOHandler;
class IPAddress;
class Sockets;

// The HTTPProxy class implements a simple web proxy that
// is bound to a specific interface and name server.  This
// allows us to specify which connection a URL should be
// fetched through, even though many connections
// could be active at the same time.
//
// This service is meant to be low-performance, since we
// do not want to divert resources from the rest of the
// connection manager.  As such, we serve one client request
// at a time.  This is probably okay since the use case is
// limited -- only portal detection, activation and Cashew
// are planned to be full-time users.
class HTTPProxy {
 public:
  enum State {
    kStateIdle,
    kStateWaitConnection,
    kStateReadClientHeader,
    kStateLookupServer,
    kStateConnectServer,
    kStateTunnelData,
    kStateFlushResponse,
  };

  explicit HTTPProxy(ConnectionRefPtr connection);
  virtual ~HTTPProxy();

  // Start HTTP proxy.
  bool Start(EventDispatcher *dispatcher, Sockets *sockets);

  // Shutdown.
  void Stop();

  int proxy_port() const { return proxy_port_; }

 private:
  friend class HTTPProxyTest;

  // Time to wait for initial headers from client.
  static const int kClientHeaderTimeoutSeconds;
  // Time to wait for connection to remote server.
  static const int kConnectTimeoutSeconds;
  // Time to wait for DNS server.
  static const int kDNSTimeoutSeconds;
  // Default port on remote server to connect to.
  static const int kDefaultServerPort;
  // Time to wait for any input from either server or client.
  static const int kInputTimeoutSeconds;
  // Maximum clients to be kept waiting.
  static const size_t kMaxClientQueue;
  // Maximum number of header lines to accept.
  static const size_t kMaxHeaderCount;
  // Maximum length of an individual header line.
  static const size_t kMaxHeaderSize;
  // Timeout for whole transaction.
  static const int kTransactionTimeoutSeconds;

  static const char kHTTPMethodConnect[];
  static const char kHTTPMethodTerminator[];
  static const char kHTTPURLDelimiters[];
  static const char kHTTPURLPrefix[];
  static const char kHTTPVersionPrefix[];
  static const char kHTTPVersionErrorMsg[];
  static const char kInternalErrorMsg[];  // Message to send on failure.

  void AcceptClient(int fd);
  bool ConnectServer(const IPAddress &address, int port);
  void GetDNSResult(const Error &error, const IPAddress &address);
  void OnReadError(const Error &error);
  void OnConnectCompletion(bool success, int fd);
  bool ParseClientRequest();
  bool ProcessLastHeaderLine();
  bool ReadClientHeaders(InputData *data);
  bool ReadClientHostname(std::string *header);
  bool ReadClientHTTPMethod(std::string *header);
  bool ReadClientHTTPVersion(std::string *header);
  void ReadFromClient(InputData *data);
  void ReadFromServer(InputData *data);
  void SetClientResponse(int code, const std::string &type,
                         const std::string &content_type,
                         const std::string &message);
  void SendClientError(int code, const std::string &error);
  void StartIdleTimeout();
  void StartReceive();
  void StartTransmit();
  void StopClient();
  void WriteToClient(int fd);
  void WriteToServer(int fd);

  // State held for the lifetime of the proxy.
  State state_;
  ConnectionRefPtr connection_;
  base::WeakPtrFactory<HTTPProxy> weak_ptr_factory_;
  base::Callback<void(int)> accept_callback_;
  base::Callback<void(bool, int)> connect_completion_callback_;
  base::Callback<void(const Error &, const IPAddress &)> dns_client_callback_;
  base::Callback<void(InputData *)> read_client_callback_;
  base::Callback<void(InputData *)> read_server_callback_;
  base::Callback<void(int)> write_client_callback_;
  base::Callback<void(int)> write_server_callback_;

  // State held while proxy is started (even if no transaction is active).
  scoped_ptr<IOHandler> accept_handler_;
  EventDispatcher *dispatcher_;
  scoped_ptr<DNSClient> dns_client_;
  int proxy_port_;
  int proxy_socket_;
  scoped_ptr<AsyncConnection> server_async_connection_;
  Sockets *sockets_;

  // State held while proxy is started and a transaction is active.
  int client_socket_;
  std::string client_method_;
  std::string client_version_;
  int server_port_;
  int server_socket_;
  bool is_route_requested_;
  base::CancelableClosure idle_timeout_;
  base::CancelableClosure transaction_timeout_;
  std::vector<std::string> client_headers_;
  std::string server_hostname_;
  ByteString client_data_;
  ByteString server_data_;
  scoped_ptr<IOHandler> read_client_handler_;
  scoped_ptr<IOHandler> write_client_handler_;
  scoped_ptr<IOHandler> read_server_handler_;
  scoped_ptr<IOHandler> write_server_handler_;

  DISALLOW_COPY_AND_ASSIGN(HTTPProxy);
};

}  // namespace shill

#endif  // SHILL_HTTP_PROXY_H_
