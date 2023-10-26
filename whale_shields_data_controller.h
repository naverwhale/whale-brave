/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WHALE_WHALE_BROWSER_UI_WHALE_SHIELDS_DATA_CONTROLLER_H_
#define WHALE_WHALE_BROWSER_UI_WHALE_SHIELDS_DATA_CONTROLLER_H_

#include <set>
#include <string>
#include <vector>

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

enum BlockType {
  Ads = 0,
  Trackers,
  URLParams,
};

namespace whale {

class WhaleShieldsDataController
    : public content::WebContentsObserver,
      public content::WebContentsUserData<WhaleShieldsDataController>,
      public content_settings::Observer {
 public:
  WhaleShieldsDataController(const WhaleShieldsDataController&) = delete;
  WhaleShieldsDataController& operator=(const WhaleShieldsDataController&) =
      delete;
  ~WhaleShieldsDataController() override;

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnResourcesChanged() = 0;
    virtual void OnTrackingBlockerEnabledChanged() {}
  };

  void HandleItemBlocked(const BlockType& block_type,
                         const std::string& subresource);
  void HandleURLParamsBlocked(const std::vector<std::string>& params);
  void ClearAllResourcesList();
  int GetTotalBlockedCount();
  int GetBlockedTrackersCount();
  int GetBlockedAdsCount();
  bool GetTrackingBlockerEnabled();
  void SetTrackingBlockerEnabled(bool is_enabled);
  bool GetAdBlockerEnabled();
  void SetAdBlockerEnabled(bool is_enabled);
  GURL GetCurrentSiteURL();
  std::vector<std::string> GetBlockedAdsList();
  std::vector<std::string> GetBlockedTrackersList();

 private:
  friend class content::WebContentsUserData<WhaleShieldsDataController>;

  explicit WhaleShieldsDataController(content::WebContents* web_contents);

  void ReloadWebContents();

  // content::WebContentsObserver
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void WebContentsDestroyed() override;

  // content_settings::Observer
  void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsTypeSet content_type_set) override;

  base::ObserverList<Observer> observer_list_;
  std::set<std::string> resource_list_blocked_ads_;
  std::set<std::string> resource_list_blocked_trackers_;
  std::set<std::string> resource_list_blocked_url_params_;

  bool keep_blocked_url_params_record_ = false;
  base::ScopedObservation<HostContentSettingsMap, content_settings::Observer>
      observation_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace whale

#endif  // WHALE_BROWSER_UI_WHALE_SHIELDS_DATA_CONTROLLER_H_
