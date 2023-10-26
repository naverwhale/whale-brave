/* Copyright 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WHALE_COMPONENTS_TRACKING_BLOCKERS_COMMON_TRACKING_BLOCKER_UTILS_H_
#define WHALE_COMPONENTS_TRACKING_BLOCKERS_COMMON_TRACKING_BLOCKER_UTILS_H_

#include "components/content_settings/core/common/content_settings.h"

class GURL;

namespace whale_blocker {

ContentSetting GetTrackingBlockerContentSettingFromRules(
    const ContentSettingsForOneType& rules,
    const GURL& primary_url);

#endif  // WHALE_COMPONENTS_TRACKING_BLOCKERS_COMMON_TRACKING_BLOCKER_UTILS_H_

}  // namespace whale_blocker
