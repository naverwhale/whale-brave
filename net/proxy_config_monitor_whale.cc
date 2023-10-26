// Copyright (c) 2020 NAVER Corp. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "components/prefs/pref_registry_simple.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "whale/base/nelo_log.h"
#include "whale/whale/browser/ui/net/pac_script_download_timeout_infobar_delegate.h"
#endif  // BUILDFLAG(IS_WIN)

class Profile;

namespace {

// This value is from proxy_resolver_factory_mojo_whale.cc.
constexpr int32_t kPACDownloadTimeoutId = 0xff001bad;
constexpr int32_t kPACDownloadSuccessId = 0xff001004;

#if BUILDFLAG(IS_WIN)
// Integer pref that specify the number of occurrences of PAC script download
// timeout.
constexpr char kWhalePACScriptDownloadTimeoutCountPref[] =
    "whale.pac_script_download_timeout_count";

// Show info bar if the number of timeout occurrences is over this value.
constexpr int kThresholdCount = 5;

int IncrementTimeoutCountFromPref() {
  int new_count = g_browser_process->local_state()->GetInteger(
                      kWhalePACScriptDownloadTimeoutCountPref) +
                  1;
  g_browser_process->local_state()->SetInteger(
      kWhalePACScriptDownloadTimeoutCountPref, new_count);
  return new_count;
}

void ResetTimeoutCountFromPref() {
  g_browser_process->local_state()->ClearPref(
      kWhalePACScriptDownloadTimeoutCountPref);
}

bool MaybeShowInfobar() {
  Browser* browser = chrome::FindLastActive();
  if (!browser)
    return false;
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return false;

  int count = IncrementTimeoutCountFromPref();
  VLOG(1) << "PAC script download timeout count: " << count;
  if (count >= kThresholdCount) {
    PACScriptDownloadTimeoutInfoBarDelegate::Create(
        infobars::ContentInfoBarManager::FromWebContents(web_contents));
    ResetTimeoutCountFromPref();
    return true;
  }
  return false;
}
#endif  // BUILDFLAG(IS_WIN)

void OnPACScriptDownloadTimeout(int32_t line_number, Profile* profile) {
#if BUILDFLAG(IS_WIN)
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // According to nelo logs, win10 is the only platform that shows this problem.
  if (base::win::GetVersion() < base::win::Version::WIN10)
    return;
  // Ignore ProxyConfigMonitor associated with a profile.
  if (profile) {
    return;
  }

  // This function can be called more than once on browser startup because
  // ProxyConfigMonitor can be instantiated not only for SystemNetworkContext
  // but also SafeBrowsingService. Prevent the timeout count from being
  // incremented by more than one per browser launch.
  static bool already_counted = false;
  if (already_counted) {
    return;
  }
  already_counted = true;

  bool success = line_number == kPACDownloadSuccessId;
  if (success) {
    ResetTimeoutCountFromPref();
    VLOG(1) << "PAC script download success";
    return;
  }
  if (MaybeShowInfobar()) {
    NELO_LOG(VERBOSE) << "PAC script download timeout info bar is created";
  }
#endif  // BUILDFLAG(IS_WIN)
}

}  // namespace

#define ON_PAC_SCRIPT_ERROR_WHALE                      \
  if (line_number == kPACDownloadTimeoutId ||          \
      line_number == kPACDownloadSuccessId) {          \
    OnPACScriptDownloadTimeout(line_number, profile_); \
    return;                                            \
  }

#include "chrome/browser/net/proxy_config_monitor.cc"

#undef ON_PAC_SCRIPT_ERROR_WHALE

// static
void ProxyConfigMonitor::RegisterLocalState(PrefRegistrySimple* registry) {
#if BUILDFLAG(IS_WIN)
  registry->RegisterIntegerPref(kWhalePACScriptDownloadTimeoutCountPref, 0);
#endif
}
