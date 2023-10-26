/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "whale/components/tracking_blockers/tracking_blockers_util.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "content/public/common/referrer.h"
#include "services/network/public/mojom/referrer_policy.mojom.h"
#include "url/gurl.h"

namespace whale_blocker {

namespace {

ContentSetting GetDefaultBlockFromControlType(ControlType type) {
  if (type == ControlType::DEFAULT) {
    return CONTENT_SETTING_DEFAULT;
  }

  return type == ControlType::ALLOW ? CONTENT_SETTING_ALLOW
                                    : CONTENT_SETTING_BLOCK;
}

}  // namespace

ContentSettingsPattern GetPatternFromURL(const GURL& url) {
  DCHECK(url.is_empty() ? url.possibly_invalid_spec() == "" : url.is_valid());
  if (url.is_empty() && url.possibly_invalid_spec() == "") {
    return ContentSettingsPattern::Wildcard();
  }
  return ContentSettingsPattern::FromString("*://" + url.host() + "/*");
}

void SetTrackingBlockerControlType(HostContentSettingsMap* map,
                                   ControlType type,
                                   const GURL& url) {
  if (url.is_valid() && !url.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  content_settings::SettingInfo setting_info;
  ContentSetting content_setting;
  auto primary_pattern = GetPatternFromURL(url);

  if (!primary_pattern.IsValid()) {
    return;
  }

  if (type == ControlType::DEFAULT || type == ControlType::BLOCK_THIRD_PARTY) {
    // TODO(jwoo.park): CONTENT_SETTING_ASK is default mode but the name is so
    // ambiguous. CONTENT_SETTING_ALLOW : Allow any trackers and
    // fingerprintings. CONTENT_SETTING_ASK : Default mode, blocks trackers.
    // CONTENT_SETTING_BLOCK : In addition to ASK, it blocks fingerprintings.
    content_setting = CONTENT_SETTING_ASK;
  } else {
    content_setting = GetDefaultBlockFromControlType(type);
  }

  map->SetContentSettingCustomScope(
      primary_pattern, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::TRACKING_BLOCKER, content_setting);
}

void ResetTrackingBlockerEnabled(HostContentSettingsMap* map, const GURL& url) {
  if (url.is_valid() && !url.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  auto primary_pattern = GetPatternFromURL(url);

  if (!primary_pattern.IsValid()) {
    return;
  }

  map->SetContentSettingCustomScope(
      primary_pattern, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::TRACKING_BLOCKER, CONTENT_SETTING_DEFAULT);
}

bool GetTrackingBlockerEnabled(HostContentSettingsMap* map, const GURL& url) {
#if BUILDFLAG(IS_ANDROID)
  return false;
#else
  if (url.is_valid() && !url.SchemeIsHTTPOrHTTPS()) {
    return false;
  }

  ContentSetting setting = map->GetContentSetting(
      url, GURL(), ContentSettingsType::TRACKING_BLOCKER);

  // see EnableBraveShields - allow and default == true
  return setting == CONTENT_SETTING_ALLOW ? false : true;
#endif
}

bool IsTrackingBlockerMaxLevel(HostContentSettingsMap* map, const GURL& url) {
#if BUILDFLAG(IS_ANDROID)
  return false;
#else
  if (url.is_valid() && !url.SchemeIsHTTPOrHTTPS()) {
    return false;
  }

  ContentSetting setting = map->GetContentSetting(
      url, GURL(), ContentSettingsType::TRACKING_BLOCKER);

  return setting == CONTENT_SETTING_BLOCK;
#endif
}

bool IsSameOriginNavigation(const GURL& referrer, const GURL& target_url) {
  const url::Origin original_referrer = url::Origin::Create(referrer);
  const url::Origin target_origin = url::Origin::Create(target_url);

  return original_referrer.IsSameOriginWith(target_origin);
}

bool MaybeChangeReferrer(const GURL& current_referrer,
                         const GURL& target_url,
                         content::Referrer* output_referrer,
                         HostContentSettingsMap* map) {
  DCHECK(output_referrer);

  if (map && !IsTrackingBlockerMaxLevel(map, target_url)) {
    return false;
  }

  if (current_referrer.is_empty()) {
    return false;
  }

  if (IsSameOriginNavigation(current_referrer, target_url)) {
    // Do nothing for same-origin requests. This check also prevents us from
    // sending referrer from HTTPS to HTTP.
    return false;
  }

  if (current_referrer.DomainIs("naver.com")) {
    // We always allow referrer on naver domains.
    // https://oss.navercorp.com/whale/whale/pull/36178
    return false;
  }

  // Cap the referrer to "strict-origin-when-cross-origin". More restrictive
  // policies should be already applied.
  // See https://github.com/brave/brave-browser/issues/13464
  url::Origin current_referrer_origin = url::Origin::Create(current_referrer);
  *output_referrer = content::Referrer::SanitizeForRequest(
      target_url,
      content::Referrer(
          current_referrer_origin.GetURL(),
          network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin));

  return true;
}

}  // namespace whale_blocker
