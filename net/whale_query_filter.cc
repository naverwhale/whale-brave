/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "whale/whale/browser/net/whale_query_filter.h"

#include "base/containers/fixed_flat_map.h"
#include "base/containers/fixed_flat_set.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"
#include "whale/whale/browser/net/whale_url_context.h"

namespace {

enum class TrackingQueryType {
  kUTM = 0,
  kFBCLID = 1,
  kGCLID = 2,
  kDCLID = 3,
  kTWCLID = 4,
  kIGSHID = 5,
  kMTK_TOK = 6,
  kETC = 500,
  kMaxValue = kETC,
};

TrackingQueryType StringToTrackingQueryType(const base::StringPiece& type) {
  if (type == "utm_source") {
    return TrackingQueryType::kUTM;
  }
  if (type == "fbclid") {
    return TrackingQueryType::kFBCLID;
  }
  if (type == "gclid") {
    return TrackingQueryType::kGCLID;
  }
  if (type == "dclid") {
    return TrackingQueryType::kDCLID;
  }
  if (type == "twclid") {
    return TrackingQueryType::kTWCLID;
  }
  if (type == "igshid") {
    return TrackingQueryType::kIGSHID;
  }
  if (type == "mkt_tok") {
    return TrackingQueryType::kMTK_TOK;
  }
  return TrackingQueryType::kETC;
}

}  // namespace

static constexpr auto kSimpleQueryStringTrackers =
    base::MakeFixedFlatSet<base::StringPiece>(
        {// https://github.com/brave/brave-browser/issues/4239
         "fbclid", "gclid", "msclkid", "mc_eid",
         // https://github.com/brave/brave-browser/issues/9879
         "dclid",
         // https://github.com/brave/brave-browser/issues/13644
         "oly_anon_id", "oly_enc_id",
         // https://github.com/brave/brave-browser/issues/11579
         "_openstat",
         // https://github.com/brave/brave-browser/issues/11817
         "vero_conv", "vero_id",
         // https://github.com/brave/brave-browser/issues/13647
         "wickedid",
         // https://github.com/brave/brave-browser/issues/11578v
         "yclid",
         // https://github.com/brave/brave-browser/issues/8975
         "__s",
         // https://github.com/brave/brave-browser/issues/17451
         "rb_clickid",
         // https://github.com/brave/brave-browser/issues/17452
         "s_cid",
         // https://github.com/brave/brave-browser/issues/17507
         "ml_subscriber", "ml_subscriber_hash",
         // https://github.com/brave/brave-browser/issues/18020
         "twclid",
         // https://github.com/brave/brave-browser/issues/18758
         "gbraid", "wbraid",
         // https://github.com/brave/brave-browser/issues/9019
         "_hsenc", "__hssc", "__hstc", "__hsfp", "hsCtaTracking",
         // https://github.com/brave/brave-browser/issues/22082
         "oft_id", "oft_k", "oft_lk", "oft_d", "oft_c", "oft_ck", "oft_ids",
         "oft_sk",
         // https://github.com/brave/brave-browser/issues/24988
         "ss_email_id",
         // https://github.com/brave/brave-browser/issues/25238
         "bsft_uid", "bsft_clkid",
         // https://github.com/brave/brave-browser/issues/25691
         "guce_referrer", "guce_referrer_sig",
         // https://github.com/brave/brave-browser/issues/26295
         "vgo_ee"});

static constexpr auto kConditionalQueryStringTrackers =
    base::MakeFixedFlatMap<base::StringPiece, base::StringPiece>(
        {// https://github.com/brave/brave-browser/issues/9018
         {"mkt_tok", "([uU]nsubscribe|emailWebview)"}});

static constexpr auto kScopedQueryStringTrackers =
    base::MakeFixedFlatMap<base::StringPiece, base::StringPiece>({
        // https://github.com/brave/brave-browser/issues/11580
        {"igshid", "instagram.com"},
        // https://github.com/brave/brave-browser/issues/26966
        {"ref_src", "twitter.com"},
        {"ref_url", "twitter.com"},
    });

// Remove tracking query parameters from a GURL, leaving all
// other parts untouched.
absl::optional<std::string> StripQueryParameter(
    const base::StringPiece& query,
    const std::string& spec,
    std::vector<std::string>& removed_tracker) {
  // We are using custom query string parsing code here. See
  // https://github.com/brave/brave-core/pull/13726#discussion_r897712350
  // for more information on why this approach was selected.
  //
  // Split query string by ampersands, remove tracking parameters,
  // then join the remaining query parameters, untouched, back into
  // a single query string.
  const std::vector<base::StringPiece> input_kv_strings =
      SplitStringPiece(query, "&", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  std::vector<base::StringPiece> output_kv_strings;
  int disallowed_count = 0;
  for (const auto& kv_string : input_kv_strings) {
    const std::vector<base::StringPiece> pieces = SplitStringPiece(
        kv_string, "=", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    const base::StringPiece& key = pieces.empty() ? "" : pieces[0];
    if (pieces.size() >= 2 &&
        (kSimpleQueryStringTrackers.count(key) == 1 ||
         (kScopedQueryStringTrackers.count(key) == 1 &&
          GURL(spec).DomainIs(kScopedQueryStringTrackers.at(key).data())) ||
         (kConditionalQueryStringTrackers.count(key) == 1 &&
          !re2::RE2::PartialMatch(
              spec, kConditionalQueryStringTrackers.at(key).data())))) {
      ++disallowed_count;

      UMA_HISTOGRAM_ENUMERATION("Whale.ITP.URLQueryFiltering",
                                StringToTrackingQueryType(key));
      removed_tracker.push_back(std::string(key.begin(), key.end()));
    } else {
      output_kv_strings.push_back(kv_string);
    }
  }
  if (disallowed_count > 0) {
    return base::JoinString(output_kv_strings, "&");
  }
  return absl::nullopt;
}

void ApplyPotentialQueryStringFilter(
    std::shared_ptr<WhaleRequestInfo> ctx,
    std::vector<std::string>& removed_tracker) {
  // TODO(jwoo.park): Need to keep track the benchmark Brave browser's UMA.
  // There histogram is "Brave.SiteHacks.QueryFilter".
  SCOPED_UMA_HISTOGRAM_TIMER("Whale.ITP.SiteHacks.QueryFilter");
  if (!ctx->enable_tracking_blocker) {
    // Don't apply the filter if the destination URL has shields down.
    return;
  }

  if (ctx->method != "GET") {
    return;
  }

  if (ctx->redirect_source.is_valid()) {
    if (ctx->internal_redirect) {
      // Ignore internal redirects since we trigger them.
      return;
    }

    if (net::registry_controlled_domains::SameDomainOrHost(
            ctx->redirect_source, ctx->request_url,
            net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
      // Same-site redirects are exempted.
      return;
    }
  } else if (ctx->initiator_url.is_valid() &&
             net::registry_controlled_domains::SameDomainOrHost(
                 ctx->initiator_url, ctx->request_url,
                 net::registry_controlled_domains::
                     INCLUDE_PRIVATE_REGISTRIES)) {
    // Same-site requests are exempted.
    return;
  }
  auto filtered_url = ApplyQueryFilter(ctx->request_url, removed_tracker);
  if (filtered_url.has_value()) {
    ctx->new_url_spec = filtered_url.value().spec();
  }
}

absl::optional<GURL> ApplyQueryFilter(
    const GURL& original_url,
    std::vector<std::string>& removed_tracker) {
  const auto& query = original_url.query_piece();
  const std::string& spec = original_url.spec();
  const auto clean_query_value =
      StripQueryParameter(query, spec, removed_tracker);
  if (!clean_query_value.has_value()) {
    return absl::nullopt;
  }
  const auto& clean_query = clean_query_value.value();
  if (clean_query.length() < query.length()) {
    GURL::Replacements replacements;
    if (clean_query.empty()) {
      replacements.ClearQuery();
    } else {
      replacements.SetQueryStr(clean_query);
    }
    return original_url.ReplaceComponents(replacements);
  }
  return absl::nullopt;
}
