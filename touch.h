/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WHALE_THIRD_PARTY_BLINK_RENDERER_CORE_INPUT_TOUCH_H_
#define WHALE_THIRD_PARTY_BLINK_RENDERER_CORE_INPUT_TOUCH_H_

#include "whale/third_party/blink/renderer/core/farbling/whale_session_cache.h"

#define screenX                                                                \
  screenX() const {                                                            \
    return whale::FarbledPointerScreenCoordinate(                              \
        target()->ToDOMWindow(), whale::FarbleKey::kPointerScreenX, clientX(), \
        screenX_ChromiumImpl());                                               \
  }                                                                            \
  double screenX_ChromiumImpl

#define screenY                                                                \
  screenY() const {                                                            \
    return whale::FarbledPointerScreenCoordinate(                              \
        target()->ToDOMWindow(), whale::FarbleKey::kPointerScreenY, clientY(), \
        screenY_ChromiumImpl());                                               \
  }                                                                            \
  double screenY_ChromiumImpl

#include "src/third_party/blink/renderer/core/input/touch.h"

#undef screenX
#undef screenY

#endif  // WHALE_CHROMIUM_SRC_THIRD_PARTY_BLINK_RENDERER_CORE_INPUT_TOUCH_H_
