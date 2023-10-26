/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "whale/services/network/restricted_cookie_manager_whale.h"

#include "whale/net/base/whale_utils.h"

#define FromStorage(NAME, VALUE, DOMAIN, PATH, CREATION, EXPIRY, LAST_ACCESS, \
                    LAST_UPDATE, SECURE, HTTP_ONLY, SAME_SITE, PRIORITY,      \
                    SAME_PARTY, PARTITION, SOURCE_SCHEME, PORT)               \
  FromStorage(NAME, VALUE, DOMAIN, PATH, CREATION,                            \
              ModifyExpiration(EXPIRY, CREATION), LAST_ACCESS, LAST_UPDATE,   \
              SECURE, HTTP_ONLY, SAME_SITE, PRIORITY, SAME_PARTY, PARTITION,  \
              SOURCE_SCHEME, PORT)

#include "services/network/restricted_cookie_manager.cc"

namespace {

constexpr base::TimeDelta kMaxCookieExpiration =
    base::Days(7);  // For JS cookies: CookieStore and document.cookie

}  // namespace

namespace network {

// Modify client side cookies to 7 days expiration.
base::Time RestrictedCookieManager::ModifyExpiration(
    const base::Time& expiry_date,
    const base::Time& creation_date) const {
  if (!whale_utility::GetCookieExpirationEnabled()) {
    return expiry_date;
  }
  const base::Time max_expiration = creation_date + kMaxCookieExpiration;

  return std::min(expiry_date, max_expiration);
}

}  // namespace network
