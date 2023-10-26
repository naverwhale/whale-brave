/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WHALE_COMPONENTS_TRACKING_BLOCKERS_TRACKING_BLOCKERS_UTIL_H_
#define WHALE_COMPONENTS_TRACKING_BLOCKERS_TRACKING_BLOCKERS_UTIL_H_

#include <stdint.h>

namespace content {
struct Referrer;
}

class HostContentSettingsMap;
class GURL;

namespace whale_blocker {

enum ControlType { ALLOW = 0, BLOCK, BLOCK_THIRD_PARTY, DEFAULT, INVALID };

void SetTrackingBlockerControlType(HostContentSettingsMap* map,
                                   ControlType type,
                                   const GURL& url);
// reset to the default value
void ResetTrackingBlockerEnabled(HostContentSettingsMap* map, const GURL& url);
bool GetTrackingBlockerEnabled(HostContentSettingsMap* map, const GURL& url);
bool IsTrackingBlockerMaxLevel(HostContentSettingsMap* map, const GURL& url);

bool MaybeChangeReferrer(const GURL& current_referrer,
                         const GURL& target_url,
                         content::Referrer* output_referrer,
                         HostContentSettingsMap* map = nullptr);

}  // namespace whale_blocker

#endif  // WHALE_COMPONENTS_TRACKING_BLOCKERS_TRACKING_BLOCKERS_UTIL_H_
