// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_basic_state.h"

#include <set>
#include <utility>

#include "base/check_op.h"
#include "base/no_destructor.h"
#include "net/base/io_buffer.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_body_drainer.h"
#include "net/http/http_stream_parser.h"
#include "net/http/http_util.h"
#include "net/socket/client_socket_handle.h"
#include "url/gurl.h"

namespace net {

HttpBasicState::HttpBasicState(std::unique_ptr<ClientSocketHandle> connection,
                               bool is_for_get_to_http_proxy)
    : read_buf_(base::MakeRefCounted<GrowableIOBuffer>()),
      connection_(std::move(connection)),
      is_for_get_to_http_proxy_(is_for_get_to_http_proxy) {
  CHECK(connection_) << "ClientSocketHandle passed to HttpBasicState must "
                        "not be NULL. See crbug.com/790776";
}

HttpBasicState::~HttpBasicState() = default;

void HttpBasicState::Initialize(const HttpRequestInfo* request_info,
                                RequestPriority priority,
                                const NetLogWithSource& net_log) {
  DCHECK(!parser_.get());
  traffic_annotation_ = request_info->traffic_annotation;
  parser_ = std::make_unique<HttpStreamParser>(
      connection_->socket(), connection_->is_reused(), request_info->url,
      request_info->method, request_info->upload_data_stream, read_buf_.get(),
      net_log);
}

std::unique_ptr<ClientSocketHandle> HttpBasicState::ReleaseConnection() {
  return std::move(connection_);
}

scoped_refptr<GrowableIOBuffer> HttpBasicState::read_buf() const {
  return read_buf_;
}

void HttpBasicState::DeleteParser() { parser_.reset(); }

std::string HttpBasicState::GenerateRequestLine() const {
  static const char kSuffix[] = " HTTP/1.1\r\n";
  const size_t kSuffixLen = std::size(kSuffix) - 1;
  const std::string path = is_for_get_to_http_proxy_
                               ? HttpUtil::SpecForRequest(parser_->url())
                               : parser_->url().PathForRequest();
  // Don't use StringPrintf for concatenation because it is very inefficient.
  std::string request_line;
  const size_t expected_size =
      parser_->method().size() + 1 + path.size() + kSuffixLen;
  request_line.reserve(expected_size);
  request_line.append(parser_->method());
  request_line.append(1, ' ');
  request_line.append(path);
  request_line.append(kSuffix, kSuffixLen);
  DCHECK_EQ(expected_size, request_line.size());
  return request_line;
}

bool HttpBasicState::IsConnectionReused() const {
  return connection_->is_reused() ||
         connection_->reuse_type() == ClientSocketHandle::UNUSED_IDLE;
}

const std::set<std::string>& HttpBasicState::GetDnsAliases() const {
  static const base::NoDestructor<std::set<std::string>> emptyset_result;
  return (connection_ && connection_->socket())
             ? connection_->socket()->GetDnsAliases()
             : *emptyset_result;
}

}  // namespace net
