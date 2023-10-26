/* Copyright 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "whale/components/tracking_blockers/common/tracking_blocker_utils.h"

#include <set>
#include <string>

#include "components/content_settings/core/common/content_settings.h"
#include "url/gurl.h"

namespace whale_blocker {

ContentSetting GetTrackingBlockerContentSettingFromRules(
    const ContentSettingsForOneType& rules,
    const GURL& primary_url) {
  for (const auto& rule : rules) {
    if (rule.primary_pattern.Matches(primary_url)) {
      return rule.GetContentSetting();
    }
  }

  return CONTENT_SETTING_DEFAULT;
}

}  // namespace whale_blocker
