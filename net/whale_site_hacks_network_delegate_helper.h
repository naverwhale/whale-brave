/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WHALE_WHALE_BROWSER_NET_WHALE_SITE_HACKS_NETWORK_DELEGATE_HELPER_H_
#define WHALE_WHALE_BROWSER_NET_WHALE_SITE_HACKS_NETWORK_DELEGATE_HELPER_H_

#include <memory>

#include "content/public/browser/browser_thread.h"
#include "whale/whale/browser/net/whale_url_context.h"


bool ApplyPotentialReferrerBlock(std::shared_ptr<WhaleRequestInfo> ctx);
int OnBeforeURLRequest_SiteHacksWork(
    std::shared_ptr<WhaleRequestInfo> ctx, raw_ptr<GURL> new_url);

#endif  // WHALE_WHALE_BROWSER_NET_WHALE_SITE_HACKS_NETWORK_DELEGATE_HELPER_H_
