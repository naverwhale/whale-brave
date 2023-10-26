/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WHALE_WHALE_BROWSER_NET_WHALE_URL_CONTEXT_H_
#define WHALE_WHALE_BROWSER_NET_WHALE_URL_CONTEXT_H_

#include <memory>
#include <set>
#include <string>

#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/referrer_policy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "url/gurl.h"

class WhaleRequestHandler;

namespace content {
class BrowserContext;
}

namespace network {
struct ResourceRequest;
}

struct WhaleRequestInfo;
using ResponseCallback = base::RepeatingCallback<void()>;

struct WhaleRequestInfo {
  WhaleRequestInfo();
  WhaleRequestInfo(const WhaleRequestInfo&) = delete;
  WhaleRequestInfo& operator=(const WhaleRequestInfo&) = delete;

  // For tests, should not be used directly.
  explicit WhaleRequestInfo(const GURL& url);

  ~WhaleRequestInfo();
  std::string method;
  GURL request_url;
  GURL tab_origin;
  GURL tab_url;
  GURL initiator_url;

  bool internal_redirect = false;
  GURL redirect_source;

  GURL referrer;
  net::ReferrerPolicy referrer_policy =
      net::ReferrerPolicy::CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE;
  absl::optional<GURL> new_referrer;

  absl::optional<int> pending_error;
  std::string new_url_spec;

  bool enable_tracking_blocker = true;
  bool allow_http_upgradable_resource = false;
  bool allow_referrers = false;
  bool is_webtorrent_disabled = false;
  int frame_tree_node_id = 0;
  uint64_t request_identifier = 0;
  size_t next_url_request_index = 0;

  raw_ptr<content::BrowserContext> browser_context = nullptr;
  raw_ptr<net::HttpRequestHeaders> headers = nullptr;
  // The following two sets are populated by |OnBeforeStartTransactionCallback|.
  // |set_headers| contains headers which values were added or modified.
  std::set<std::string> set_headers;
  std::set<std::string> removed_headers;
  raw_ptr<const net::HttpResponseHeaders> original_response_headers = nullptr;
  raw_ptr<scoped_refptr<net::HttpResponseHeaders>> override_response_headers = nullptr;

  raw_ptr<GURL> new_url = nullptr;

  // Default to invalid type for resource_type, so delegate helpers
  // can properly detect that the info couldn't be obtained.
  // TODO(iefremov): Replace with something like |WebRequestResourceType| to
  // distinguish WebSockets.
  static constexpr blink::mojom::ResourceType kInvalidResourceType =
      static_cast<blink::mojom::ResourceType>(-1);
  blink::mojom::ResourceType resource_type = kInvalidResourceType;

  static std::shared_ptr<WhaleRequestInfo> MakeCTX(
      const network::ResourceRequest& request,
      int render_process_id,
      int frame_tree_node_id,
      uint64_t request_identifier,
      content::BrowserContext* browser_context,
      std::shared_ptr<WhaleRequestInfo> old_ctx);

 private:
  friend class WhaleProxyingURLLoaderFactory;
  // Please don't add any more friends here if it can be avoided.
  // We should also remove the one below.
  // friend class ::WhaleRequestHandler;
};

// ResponseListener
using OnBeforeURLRequestCallback =
    base::RepeatingCallback<int(const ResponseCallback& next_callback,
                                std::shared_ptr<WhaleRequestInfo> ctx)>;

#endif  // WHALE_WHALE_BROWSER_NET_WHALE_URL_CONTEXT_H_
