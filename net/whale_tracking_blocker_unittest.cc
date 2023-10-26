/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
// Copyright (c) 2023 NAVER Corp. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "net/base/net_errors.h"
#include "net/url_request/url_request_job.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"
#include "whale/whale/browser/net/whale_query_filter.h"
#include "whale/whale/browser/net/whale_site_hacks_network_delegate_helper.h"
#include "whale/whale/browser/net/whale_url_context.h"

TEST(WhaleTrackingBlockerTest, ReferrerPreserved) {
  const std::vector<const GURL> urls(
      {GURL("https://brianbondy.com/7"), GURL("https://www.brianbondy.com/5"),
       GURL("https://brian.bondy.brianbondy.com")});
  for (const auto& url : urls) {
    net::HttpRequestHeaders headers;
    const GURL original_referrer("https://hello.brianbondy.com/about");

    auto whale_request_info = std::make_shared<WhaleRequestInfo>(url);
    whale_request_info->referrer = original_referrer;
    whale_request_info->allow_referrers = false;
    int rc = OnBeforeURLRequest_SiteHacksWork(whale_request_info,
                                              const_cast<GURL*>(&url));
    EXPECT_EQ(rc, net::OK);
    // new_url should not be set.
    EXPECT_TRUE(whale_request_info->new_url_spec.empty());
    EXPECT_EQ(whale_request_info->referrer, original_referrer);
  }
}

TEST(WhaleTrackingBlockerTest, ReferrerTruncated) {
  const std::vector<const GURL> urls({GURL("https://digg.com/7"),
                                      GURL("https://slashdot.org/5"),
                                      GURL("https://bondy.brian.org")});
  for (const auto& url : urls) {
    const GURL original_referrer("https://hello.brianbondy.com/about");

    auto whale_request_info = std::make_shared<WhaleRequestInfo>(url);
    whale_request_info->referrer = original_referrer;
    whale_request_info->allow_referrers = false;
    int rc = OnBeforeURLRequest_SiteHacksWork(whale_request_info,
                                              const_cast<GURL*>(&url));
    EXPECT_EQ(rc, net::OK);
    // new_url should not be set.
    EXPECT_TRUE(whale_request_info->new_url_spec.empty());
    EXPECT_TRUE(whale_request_info->new_referrer.has_value());
    EXPECT_EQ(whale_request_info->new_referrer.value(),
              url::Origin::Create(original_referrer).GetURL());
  }
}

TEST(WhaleTrackingBlockerTest, ReferrerWouldBeClearedButExtensionSite) {
  const std::vector<const GURL> urls({GURL("https://digg.com/7"),
                                      GURL("https://slashdot.org/5"),
                                      GURL("https://bondy.brian.org")});
  for (const auto& url : urls) {
    auto whale_request_info = std::make_shared<WhaleRequestInfo>(url);
    whale_request_info->tab_origin =
        GURL("chrome-extension://aemmndcbldboiebfnladdacbdfmadadm/");
    const GURL original_referrer("https://hello.brianbondy.com/about");
    whale_request_info->referrer = original_referrer;
    whale_request_info->allow_referrers = false;

    int rc = OnBeforeURLRequest_SiteHacksWork(whale_request_info,
                                              const_cast<GURL*>(&url));
    EXPECT_EQ(rc, net::OK);
    // new_url should not be set
    EXPECT_TRUE(whale_request_info->new_url_spec.empty());
    EXPECT_EQ(whale_request_info->referrer, original_referrer);
  }
}

TEST(WhaleTrackingBlockerTest, QueryStringUntouched) {
  const std::vector<const std::string> urls(
      {"https://example.com/",
       "https://example.com/?",
       "https://example.com/?+%20",
       "https://user:pass@example.com/path/file.html?foo=1#fragment",
       "http://user:pass@example.com/path/file.html?foo=1&bar=2#fragment",
       "https://example.com/?file=https%3A%2F%2Fexample.com%2Ftest.pdf",
       "https://example.com/?title=1+2&caption=1%202",
       "https://example.com/?foo=1&&bar=2#fragment",
       "https://example.com/?foo&bar=&#fragment",
       "https://example.com/?foo=1&fbcid=no&gcid=no&mc_cid=no&bar=&#frag",
       "https://example.com/?fbclid=&gclid&=mc_eid&msclkid=",
       "https://example.com/?value=fbclid=1&not-gclid=2&foo+mc_eid=3",
       "https://example.com/?+fbclid=1",
       "https://example.com/?%20fbclid=1",
       "https://example.com/#fbclid=1",
       "https://example.com/1;k=v;&a=b&c=d&gclid=1234;%3fhttp://ad.co/?e=f&g=1",
       "https://example.com/?__ss=1234-abcd",
       "https://example.com/?mkt_tok=123&mkt_unsubscribe=1",
       "https://example.com/?mkt_unsubscribe=1&fake_param=abc&mkt_tok=123",
       "https://example.com/Unsubscribe.html?fake_param=abc&mkt_tok=123"});
  std::vector<std::string> result;
  for (const auto& url : urls) {
    auto whale_request_info = std::make_shared<WhaleRequestInfo>(GURL(url));
    whale_request_info->initiator_url =
        GURL("https://example.net");  // cross-site
    whale_request_info->method = "GET";
    ApplyPotentialQueryStringFilter(whale_request_info, result);

    // new_url should not be set
    EXPECT_TRUE(whale_request_info->new_url_spec.empty());
  }
}

TEST(WhaleTrackingBlockerTest, QueryStringExempted) {
  const GURL tracking_url("https://example.com/?fbclid=1");

  const std::string initiators[] = {
      "https://example.com/path",      // Same-origin
      "https://sub.example.com/path",  // Same-site
  };
  std::vector<std::string> result;

  for (const auto& initiator : initiators) {
    auto whale_request_info = std::make_shared<WhaleRequestInfo>(tracking_url);
    whale_request_info->initiator_url = GURL(initiator);
    whale_request_info->method = "GET";
    ApplyPotentialQueryStringFilter(whale_request_info, result);

    // new_url should not be set
    EXPECT_TRUE(whale_request_info->new_url_spec.empty());
  }

  // Internal redirect
  {
    auto whale_request_info = std::make_shared<WhaleRequestInfo>(tracking_url);
    whale_request_info->initiator_url =
        GURL("https://example.net");  // cross-site
    whale_request_info->method = "GET";
    whale_request_info->internal_redirect = true;
    whale_request_info->redirect_source =
        GURL("https://example.org");  // cross-site
    ApplyPotentialQueryStringFilter(whale_request_info, result);

    // new_url should not be set
    EXPECT_TRUE(whale_request_info->new_url_spec.empty());
  }

  // POST requests
  {
    auto whale_request_info = std::make_shared<WhaleRequestInfo>(tracking_url);
    whale_request_info->initiator_url =
        GURL("https://example.net");  // cross-site
    whale_request_info->method = "POST";
    whale_request_info->redirect_source =
        GURL("https://example.org");  // cross-site
    ApplyPotentialQueryStringFilter(whale_request_info, result);

    // new_url should not be set
    EXPECT_TRUE(whale_request_info->new_url_spec.empty());
  }

  // Same-site redirect
  {
    auto whale_request_info = std::make_shared<WhaleRequestInfo>(tracking_url);
    whale_request_info->initiator_url =
        GURL("https://example.net");  // cross-site
    whale_request_info->method = "GET";
    whale_request_info->redirect_source =
        GURL("https://sub.example.com");  // same-site
    ApplyPotentialQueryStringFilter(whale_request_info, result);

    // new_url should not be set
    EXPECT_TRUE(whale_request_info->new_url_spec.empty());
  }
}

TEST(WhaleTrackingBlockerTest, QueryStringFiltered) {
  const std::vector<const std::pair<const std::string, const std::string>> urls(
      {// { original url, expected url after filtering }
       {"https://example.com/?fbclid=1234", "https://example.com/"},
       {"https://example.com/?fbclid=1234&", "https://example.com/"},
       {"https://example.com/?&fbclid=1234", "https://example.com/"},
       {"https://example.com/?gclid=1234", "https://example.com/"},
       {"https://example.com/?fbclid=0&gclid=1&msclkid=a&mc_eid=a1",
        "https://example.com/"},
       {"https://example.com/?fbclid=&foo=1&bar=2&gclid=abc",
        "https://example.com/?fbclid=&foo=1&bar=2"},
       {"https://example.com/?fbclid=&foo=1&gclid=1234&bar=2",
        "https://example.com/?fbclid=&foo=1&bar=2"},
       {"http://u:p@example.com/path/file.html?foo=1&fbclid=abcd#fragment",
        "http://u:p@example.com/path/file.html?foo=1#fragment"},
       {"https://example.com/?__s=1234-abcd", "https://example.com/"},
       // Obscure edge cases that break most parsers:
       {"https://example.com/?fbclid&foo&&gclid=2&bar=&%20",
        "https://example.com/?fbclid&foo&&bar=&%20"},
       {"https://example.com/?fbclid=1&1==2&=msclkid&foo=bar&&a=b=c&",
        "https://example.com/?1==2&=msclkid&foo=bar&&a=b=c&"},
       {"https://example.com/?fbclid=1&=2&?foo=yes&bar=2+",
        "https://example.com/?=2&?foo=yes&bar=2+"},
       {"https://example.com/?fbclid=1&a+b+c=some%20thing&1%202=3+4",
        "https://example.com/?a+b+c=some%20thing&1%202=3+4"},
       // Conditional query parameter stripping
       {"https://example.com/?igshid=1234", ""},
       {"https://www.instagram.com/?igshid=1234", "https://www.instagram.com/"},
       {"https://example.com/?mkt_tok=123&foo=bar&mkt_unsubscribe=1", ""},
       {"https://example.com/index.php/email/emailWebview?mkt_tok=1234&foo=bar",
        ""},
       {"https://example.com/?mkt_tok=123&foo=bar",
        "https://example.com/?foo=bar"}});
  std::vector<std::string> result;
  for (const auto& pair : urls) {
    auto whale_request_info =
        std::make_shared<WhaleRequestInfo>(GURL(pair.first));
    whale_request_info->initiator_url =
        GURL("https://example.net");  // cross-site
    whale_request_info->method = "GET";
    ApplyPotentialQueryStringFilter(whale_request_info, result);

    EXPECT_EQ(whale_request_info->new_url_spec, pair.second);
  }

  // Cross-site redirect
  {
    auto whale_request_info = std::make_shared<WhaleRequestInfo>(
        GURL("https://example.com/?fbclid=1"));
    whale_request_info->initiator_url =
        GURL("https://example.com");  // same-origin
    whale_request_info->method = "GET";
    whale_request_info->redirect_source =
        GURL("https://example.net");  // cross-site
    ApplyPotentialQueryStringFilter(whale_request_info, result);

    EXPECT_EQ(whale_request_info->new_url_spec, "https://example.com/");
  }

  // Direct navigation
  {
    auto whale_request_info = std::make_shared<WhaleRequestInfo>(
        GURL("https://example.com/?fbclid=2"));
    whale_request_info->initiator_url = GURL();
    whale_request_info->method = "GET";
    ApplyPotentialQueryStringFilter(whale_request_info, result);

    EXPECT_EQ(whale_request_info->new_url_spec, "https://example.com/");
  }
}
