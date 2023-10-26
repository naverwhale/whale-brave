/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WHALE_WHALE_BROWSER_NET_WHALE_QUERY_FILTER_H_
#define WHALE_WHALE_BROWSER_NET_WHALE_QUERY_FILTER_H_

#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

struct WhaleRequestInfo;

absl::optional<GURL> ApplyQueryFilter(
    const GURL& original_url,
    std::vector<std::string>& removed_tracker);

void ApplyPotentialQueryStringFilter(std::shared_ptr<WhaleRequestInfo> ctx,
                                     std::vector<std::string>& removed_tracker);

#endif  // WHALE_WHALE_BROWSER_NET_WHALE_QUERY_FILTER_H_
