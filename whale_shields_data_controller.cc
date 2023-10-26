/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "whale/whale/browser/ui/whale_shields_data_controller.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"
#include "whale/components/tracking_blockers/tracking_blockers_util.h"

namespace {

HostContentSettingsMap* GetHostContentSettingsMap(
    content::WebContents* web_contents) {
  return HostContentSettingsMapFactory::GetForProfile(
      web_contents->GetBrowserContext());
}

}  // namespace

namespace whale {

WhaleShieldsDataController::~WhaleShieldsDataController() = default;

WhaleShieldsDataController::WhaleShieldsDataController(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<WhaleShieldsDataController>(*web_contents) {
  observation_.Observe(GetHostContentSettingsMap(web_contents));
}

void WhaleShieldsDataController::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInMainFrame() && navigation_handle->HasCommitted() &&
      !navigation_handle->IsSameDocument()) {
    ClearAllResourcesList();
  }
}

void WhaleShieldsDataController::WebContentsDestroyed() {
  observation_.Reset();
}

void WhaleShieldsDataController::ReloadWebContents() {
  web_contents()->GetController().Reload(content::ReloadType::NORMAL, true);
}

void WhaleShieldsDataController::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsTypeSet content_type_set) {
  if ((content_type_set.ContainsAllTypes() ||
       content_type_set.GetType() == ContentSettingsType::TRACKING_BLOCKER) &&
      primary_pattern.Matches(GetCurrentSiteURL())) {
    for (Observer& obs : observer_list_) {
      obs.OnTrackingBlockerEnabledChanged();
    }
  }
}

void WhaleShieldsDataController::ClearAllResourcesList() {
  resource_list_blocked_ads_.clear();
  resource_list_blocked_trackers_.clear();
  if (!keep_blocked_url_params_record_) {
    resource_list_blocked_url_params_.clear();
  }
  keep_blocked_url_params_record_ = false;

  for (Observer& obs : observer_list_) {
    obs.OnResourcesChanged();
  }
}

int WhaleShieldsDataController::GetTotalBlockedCount() {
  return resource_list_blocked_ads_.size() +
         resource_list_blocked_trackers_.size() +
         resource_list_blocked_url_params_.size();
}

int WhaleShieldsDataController::GetBlockedTrackersCount() {
  return resource_list_blocked_trackers_.size() +
         resource_list_blocked_url_params_.size();
}

int WhaleShieldsDataController::GetBlockedAdsCount() {
  return resource_list_blocked_ads_.size();
}

std::vector<std::string> WhaleShieldsDataController::GetBlockedAdsList() {
  std::vector<std::string> blocked_ads(resource_list_blocked_ads_.begin(),
                                       resource_list_blocked_ads_.end());

  return blocked_ads;
}

std::vector<std::string> WhaleShieldsDataController::GetBlockedTrackersList() {
  std::vector<std::string> blocked_trackers(
      resource_list_blocked_trackers_.begin(),
      resource_list_blocked_trackers_.end());

  for (auto param : resource_list_blocked_url_params_) {
    blocked_trackers.push_back(param);
  }

  return blocked_trackers;
}

bool WhaleShieldsDataController::GetTrackingBlockerEnabled() {
  return whale_blocker::GetTrackingBlockerEnabled(
      GetHostContentSettingsMap(web_contents()), GetCurrentSiteURL());
}

void WhaleShieldsDataController::SetTrackingBlockerEnabled(bool is_enabled) {
  auto* map = GetHostContentSettingsMap(web_contents());
  if (map->GetDefaultContentSetting(ContentSettingsType::TRACKING_BLOCKER,
                                    nullptr) == CONTENT_SETTING_DEFAULT) {
    whale_blocker::ResetTrackingBlockerEnabled(map, GetCurrentSiteURL());
  } else {
    whale_blocker::SetTrackingBlockerControlType(
        map,
        is_enabled ? whale_blocker::ControlType::DEFAULT
                   : whale_blocker::ControlType::ALLOW,
        GetCurrentSiteURL());
  }
  ReloadWebContents();
}

GURL WhaleShieldsDataController::GetCurrentSiteURL() {
  return web_contents()->GetLastCommittedURL();
}

void WhaleShieldsDataController::HandleItemBlocked(
    const BlockType& block_type,
    const std::string& subresource) {
  if (block_type == BlockType::Ads) {
    resource_list_blocked_ads_.insert(subresource);
  } else if (block_type == BlockType::Trackers) {
    resource_list_blocked_trackers_.insert(subresource);
  }

  for (Observer& obs : observer_list_) {
    obs.OnResourcesChanged();
  }
}

void WhaleShieldsDataController::HandleURLParamsBlocked(
    const std::vector<std::string>& params) {
  for (auto param : params) {
    resource_list_blocked_url_params_.insert(param);
  }
  keep_blocked_url_params_record_ = true;

  for (Observer& obs : observer_list_) {
    obs.OnResourcesChanged();
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(WhaleShieldsDataController);

}  // namespace whale
