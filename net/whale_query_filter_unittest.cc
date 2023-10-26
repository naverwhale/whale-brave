// Copyright (c) 2023 NAVER Corp. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "whale/whale/browser/net/whale_query_filter.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

TEST(WhaleQueryFilter, FilterQueryTrackers) {
  std::vector<std::string> result;
  // Test to filter out tracking paramaters.
  EXPECT_EQ(ApplyQueryFilter(GURL("https://test.com/?gclid=123"), result),
            GURL("https://test.com/"));
  EXPECT_EQ(ApplyQueryFilter(GURL("https://test.com/?fbclid=123"), result),
            GURL("https://test.com/"));
  EXPECT_EQ(ApplyQueryFilter(GURL("https://test.com/?mkt_tok=123"), result),
            GURL("https://test.com/"));
  EXPECT_EQ(
      ApplyQueryFilter(GURL("https://test.com/?gclid=123&unsubscribe=123"),
                       result),
      GURL("https://test.com/?unsubscribe=123"));
  EXPECT_EQ(
      ApplyQueryFilter(GURL("https://test.com/?gclid=123&Unsubscribe=123"),
                       result),
      GURL("https://test.com/?Unsubscribe=123"));

  // Test to not be filtered out on normal urls.
  EXPECT_EQ(
      ApplyQueryFilter(GURL(
          "https://n.news.naver.com/article/023/0003767882?cds=news_media_pc"),
          result),
      ApplyQueryFilter(GURL(), result));
  EXPECT_FALSE(ApplyQueryFilter(GURL("https://test.com/"), result));
  EXPECT_FALSE(ApplyQueryFilter(GURL(), result));
}
