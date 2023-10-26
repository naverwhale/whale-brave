/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "whale/whale/browser/net/whale_url_context.h"

#include <memory>
#include <string>

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/origin.h"
#include "whale/components/tracking_blockers/tracking_blockers_util.h"

WhaleRequestInfo::WhaleRequestInfo() = default;

WhaleRequestInfo::WhaleRequestInfo(const GURL& url) : request_url(url) {}

WhaleRequestInfo::~WhaleRequestInfo() = default;

// static
std::shared_ptr<WhaleRequestInfo> WhaleRequestInfo::MakeCTX(
    const network::ResourceRequest& request,
    int render_process_id,
    int frame_tree_node_id,
    uint64_t request_identifier,
    content::BrowserContext* browser_context,
    std::shared_ptr<WhaleRequestInfo> old_ctx) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto ctx = std::make_shared<WhaleRequestInfo>();
  ctx->request_identifier = request_identifier;
  ctx->method = request.method;
  ctx->request_url = request.url;
  // TODO(iefremov): Replace GURL with Origin
  ctx->initiator_url =
      request.request_initiator.value_or(url::Origin()).GetURL();

  ctx->referrer = request.referrer;
  ctx->referrer_policy = request.referrer_policy;

  ctx->resource_type =
      static_cast<blink::mojom::ResourceType>(request.resource_type);

  ctx->frame_tree_node_id = frame_tree_node_id;

  // TODO(iefremov): We still need this for WebSockets, currently
  // |AddChannelRequest| provides only old-fashioned |site_for_cookies|.
  // (See |BraveProxyingWebSocket|).
  if (ctx->tab_origin.is_empty()) {
    content::WebContents* contents =
        content::WebContents::FromFrameTreeNodeId(ctx->frame_tree_node_id);
    if (contents) {
      ctx->tab_origin =
          url::Origin::Create(contents->GetLastCommittedURL()).GetURL();
    }
  }

  if (old_ctx) {
    ctx->internal_redirect = old_ctx->internal_redirect;
    ctx->redirect_source = old_ctx->redirect_source;
  }

  Profile* profile = Profile::FromBrowserContext(browser_context);
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile);
  ctx->enable_tracking_blocker =
      whale_blocker::GetTrackingBlockerEnabled(map, ctx->tab_origin);
  ctx->allow_referrers =
      !whale_blocker::IsTrackingBlockerMaxLevel(map, ctx->tab_origin);

  ctx->browser_context = browser_context;

  // TODO(fmarier): remove this once the hacky code in
  // brave_proxying_url_loader_factory.cc is refactored. See
  // BraveProxyingURLLoaderFactory::InProgressRequest::UpdateRequestInfo().
  if (old_ctx) {
    ctx->internal_redirect = old_ctx->internal_redirect;
    ctx->redirect_source = old_ctx->redirect_source;
  }

  return ctx;
}
