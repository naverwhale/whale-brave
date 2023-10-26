/* Copyright 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WHALE_SERVICES_NETWORK_RESTRICTED_COOKIE_MANAGER_WHALE_H_
#define WHALE_SERVICES_NETWORK_RESTRICTED_COOKIE_MANAGER_WHALE_H_

#define RemoveChangeListener                                             \
  NotUsed() const {}                                                     \
  base::Time ModifyExpiration(const base::Time& expiry_date,             \
                              const base::Time& creation_date) const;    \
  void RemoveChangeListener

#include "services/network/restricted_cookie_manager.h"

#undef RemoveChangeListener

#endif  // WHALE_SERVICES_NETWORK_RESTRICTED_COOKIE_MANAGER_WHALE_H_
