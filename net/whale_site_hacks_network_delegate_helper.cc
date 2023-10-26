/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "whale/whale/browser/net/whale_site_hacks_network_delegate_helper.h"

#include <utility>

#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "content/public/common/url_constants.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/constants.h"
#include "whale/components/tracking_blockers/tracking_blockers_util.h"
#include "whale/whale/browser/net/whale_query_filter.h"
#include "whale/whale/browser/ui/whale_shields_data_controller.h"

namespace {

bool IsInternalScheme(std::shared_ptr<WhaleRequestInfo> ctx) {
  DCHECK(ctx);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (ctx->request_url.SchemeIs(extensions::kExtensionScheme)) {
    return true;
  }
#endif
  return ctx->request_url.SchemeIs(content::kChromeUIScheme);
}

} //  namespace

bool ApplyPotentialReferrerBlock(std::shared_ptr<WhaleRequestInfo> ctx) {
  if (ctx->allow_referrers) {
    return false;
  }
  if (ctx->tab_origin.SchemeIs(content::kChromeExtensionScheme)) {
    return false;
  }

  if (ctx->resource_type == blink::mojom::ResourceType::kMainFrame ||
      ctx->resource_type == blink::mojom::ResourceType::kSubFrame) {
    // Frame navigations are handled in content::NavigationRequest.
    return false;
  }

  content::Referrer new_referrer;
  if (whale_blocker::MaybeChangeReferrer(GURL(ctx->referrer),
          ctx->request_url, &new_referrer)) {
    ctx->new_referrer = new_referrer.url;
    return true;
  }
  return false;
}

int OnBeforeURLRequest_SiteHacksWork(
    std::shared_ptr<WhaleRequestInfo> ctx,
    raw_ptr<GURL> new_url) {
#if BUILDFLAG(IS_ANDROID)
  return net::OK;
#else
  ApplyPotentialReferrerBlock(ctx);
  if (IsInternalScheme(ctx)) {
    return net::OK;
  }
  ctx->new_url = new_url;

  if (ctx->request_url.DomainIs("naver.com")) {
    return net::OK;
  }

  if (ctx->request_url.has_query()) {
    std::vector<std::string> removed_trackers;
    ApplyPotentialQueryStringFilter(ctx, removed_trackers);

    if (!ctx->new_url_spec.empty() &&
        (ctx->new_url_spec != ctx->request_url.spec())) {
      *new_url = GURL(ctx->new_url_spec);
      content::WebContents* web_contents =
          content::WebContents::FromFrameTreeNodeId(ctx->frame_tree_node_id);
      if (!web_contents) {
        return net::OK;
      }
      auto* shields_data_ctrlr =
          whale::WhaleShieldsDataController::FromWebContents(web_contents);
      // |shields_data_ctrlr| can be null if the |web_contents| is generated in
      // component layer - We don't attach any tab helpers in this case.
      if (!shields_data_ctrlr) {
        return net::OK;
      }
      shields_data_ctrlr->HandleURLParamsBlocked(removed_trackers);
    }
  }
  return net::OK;
#endif
}
