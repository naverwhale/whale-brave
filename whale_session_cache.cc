/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "whale/third_party/blink/renderer/core/farbling/whale_session_cache.h"

#include "base/feature_list.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "crypto/hmac.h"
#include "third_party/blink/public/common/device_memory/approximated_device_memory.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/graphics/image_data_buffer.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/language.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "url/url_constants.h"

namespace {

constexpr uint64_t zero = 0;
constexpr double maxUInt64AsDouble = static_cast<double>(UINT64_MAX);

inline uint64_t lfsr_next(uint64_t v) {
  return ((v >> 1) | (((v << 62) ^ (v << 61)) & (~(~zero << 63) << 62)));
}

}  // namespace

namespace whale {

const char WhaleSessionCache::kSupplementName[] = "WhaleSessionCache";
const int kFarbledUserAgentMaxExtraSpaces = 5;

// acceptable letters for generating random strings
const char kLettersForRandomStrings[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789. ";
// length of kLettersForRandomStrings array
const size_t kLettersForRandomStringsLength = 64;

blink::WebContentSettingsClient* GetContentSettingsClientFor(
    ExecutionContext* context) {
  blink::WebContentSettingsClient* settings = nullptr;
  if (!context) {
    return settings;
  }
  // Avoid blocking fingerprinting in WebUI, extensions, etc.
  const String protocol = context->GetSecurityOrigin()->Protocol();
  if (protocol == url::kAboutScheme || protocol == "chrome-extension" ||
      blink::SchemeRegistry::ShouldTreatURLSchemeAsDisplayIsolated(protocol)) {
    return settings;
  }
  if (auto* window = blink::DynamicTo<blink::LocalDOMWindow>(context)) {
    auto* local_frame = window->GetFrame();
    if (!local_frame) {
      local_frame = window->GetDisconnectedFrame();
    }
    if (local_frame) {
      if (auto* top_local_frame =
              blink::DynamicTo<blink::LocalFrame>(&local_frame->Tree().Top())) {
        settings = top_local_frame->GetContentSettingsClient();
      } else {
        settings = local_frame->GetContentSettingsClient();
      }
    }
  } else if (context->IsWorkerGlobalScope()) {
    settings =
        blink::To<blink::WorkerGlobalScope>(context)->ContentSettingsClient();
  }
  return settings;
}

WhaleFarblingLevel GetWhaleFarblingLevelFor(ExecutionContext* context,
                                            WhaleFarblingLevel default_value) {
  WhaleFarblingLevel value = default_value;
  if (context) {
    value = whale::WhaleSessionCache::From(*context).GetWhaleFarblingLevel();
  }
  return value;
}

bool AllowFingerprinting(ExecutionContext* context) {
  return (GetWhaleFarblingLevelFor(context, WhaleFarblingLevel::OFF) !=
          WhaleFarblingLevel::MAXIMUM);
}

void ApplyWhaleHardwareConcurrencyOverride(blink::ExecutionContext* context,
                                           unsigned int* hardware_concurrency) {
  if (!context || AllowFingerprinting(context)) {
    return;
  }

  const unsigned kFakeMinProcessors = 2;
  const unsigned kFakeMaxProcessors = 8;
  unsigned true_value =
      static_cast<unsigned>(base::SysInfo::NumberOfProcessors());
  if (true_value <= 2) {
    *hardware_concurrency = true_value;
    return;
  }
  unsigned farbled_value = true_value;
  switch (GetWhaleFarblingLevelFor(context, WhaleFarblingLevel::OFF)) {
    case WhaleFarblingLevel::OFF: {
      break;
    }
    case WhaleFarblingLevel::MAXIMUM: {
      true_value = kFakeMaxProcessors;
      // "Maximum" behavior is "balanced" behavior but with a fake maximum,
      // so fall through here.
      [[fallthrough]];
    }
    case WhaleFarblingLevel::BALANCED: {
      whale::FarblingPRNG prng =
          whale::WhaleSessionCache::From(*context).MakePseudoRandomGenerator();
      farbled_value =
          kFakeMinProcessors + (prng() % (true_value + 1 - kFakeMinProcessors));
      break;
    }
    default:
      NOTREACHED();
  }
  *hardware_concurrency = farbled_value;
}

float FarbleDeviceMemory(blink::ExecutionContext* context) {
  float true_value =
      blink::ApproximatedDeviceMemory::GetApproximatedDeviceMemory();
  WhaleFarblingLevel farbling_level =
      whale::GetWhaleFarblingLevelFor(context, WhaleFarblingLevel::OFF);
  // If Whale Shields are down or anti-fingerprinting is off for this site,
  // return the true value.
  if (farbling_level == WhaleFarblingLevel::OFF) {
    return true_value;
  }

  std::vector<float> valid_values = {0.25, 0.5, 1.0, 2.0, 4.0, 8.0};
  size_t min_farbled_index;
  size_t max_farbled_index;
  // If anti-fingerprinting is at default level, select a pseudo-random valid
  // value between 0.5 and the true value (unless the true value is 0.25 in
  // which case just return that).
  auto true_it = base::ranges::find(valid_values, true_value);
  size_t true_index;
  // Get index into |valid_values| of the true value. If it's not found,
  // assume the last index. (This should not happen, but it allows us to
  // fail closed instead of failing open.)
  if (true_it != valid_values.end()) {
    true_index = std::distance(valid_values.begin(), true_it);
  } else {
    true_index = valid_values.size() - 1;
  }
  min_farbled_index = 1;
  max_farbled_index = true_index;
  if (max_farbled_index <= min_farbled_index) {
    return valid_values[min_farbled_index];
  }
  FarblingPRNG prng =
      WhaleSessionCache::From(*context).MakePseudoRandomGenerator();
  return valid_values[min_farbled_index +
                      (prng() % (max_farbled_index + 1 - min_farbled_index))];
}

int FarbleInteger(ExecutionContext* context,
                  whale::FarbleKey key,
                  int spoof_value,
                  int min_value,
                  int max_value) {
  WhaleSessionCache& cache = WhaleSessionCache::From(*context);
  return cache.FarbledInteger(key, spoof_value, min_value, max_value);
}

bool BlockScreenFingerprinting(ExecutionContext* context) {
  WhaleFarblingLevel level =
      GetWhaleFarblingLevelFor(context, WhaleFarblingLevel::OFF);
  return level != WhaleFarblingLevel::OFF;
}

int FarbledPointerScreenCoordinate(const DOMWindow* view,
                                   FarbleKey key,
                                   int client_coordinate,
                                   int true_screen_coordinate) {
  const blink::LocalDOMWindow* local_dom_window =
      blink::DynamicTo<blink::LocalDOMWindow>(view);
  if (!local_dom_window) {
    return true_screen_coordinate;
  }
  ExecutionContext* context = local_dom_window->GetExecutionContext();
  if (!BlockScreenFingerprinting(context)) {
    return true_screen_coordinate;
  }
  double zoom_factor = local_dom_window->GetFrame()->PageZoomFactor();
  return FarbleInteger(context, key, zoom_factor * client_coordinate, 0, 8);
}

WhaleSessionCache::WhaleSessionCache(ExecutionContext& context)
    : Supplement<ExecutionContext>(context) {
  farbling_enabled_ = false;
  farbling_level_ = WhaleFarblingLevel::OFF;
  scoped_refptr<const blink::SecurityOrigin> origin;
  if (auto* window = blink::DynamicTo<blink::LocalDOMWindow>(context)) {
    auto* frame = window->GetFrame();
    if (!frame) {
      frame = window->GetDisconnectedFrame();
    }
    if (frame) {
      origin = frame->Tree().Top().GetSecurityContext()->GetSecurityOrigin();
    }
  } else {
    origin = context.GetSecurityContext().GetSecurityOrigin();
  }
  if (!origin || origin->IsOpaque()) {
    return;
  }
  const auto host = origin->Host();
  if (host.IsNull() || host.empty()) {
    return;
  }
  const std::string domain =
      blink::network_utils::GetDomainAndRegistry(
          host, blink::network_utils::kIncludePrivateRegistries)
          .Utf8();
  if (domain.empty()) {
    return;
  }

  session_key_ = 12345;

  crypto::HMAC h(crypto::HMAC::SHA256);
  CHECK(h.Init(reinterpret_cast<const unsigned char*>(&session_key_),
               sizeof session_key_));
  CHECK(h.Sign(domain, domain_key_, sizeof domain_key_));
  const uint64_t* fudge = reinterpret_cast<const uint64_t*>(domain_key_);
  double fudge_factor = 0.99 + ((*fudge / maxUInt64AsDouble) / 100);
  uint64_t seed = *reinterpret_cast<uint64_t*>(domain_key_);
  if (blink::WebContentSettingsClient* settings =
          GetContentSettingsClientFor(&context)) {
    farbling_level_ = settings->GetWhaleFarblingLevel();
  }
  if (farbling_level_ != WhaleFarblingLevel::OFF) {
    audio_farbling_helper_.emplace(
        fudge_factor, seed, farbling_level_ == WhaleFarblingLevel::MAXIMUM);
  }
}

WhaleSessionCache& WhaleSessionCache::From(ExecutionContext& context) {
  WhaleSessionCache* cache =
      Supplement<ExecutionContext>::From<WhaleSessionCache>(context);
  if (!cache) {
    cache = blink::MakeGarbageCollected<WhaleSessionCache>(context);
    ProvideTo(context, cache);
  }
  return *cache;
}

void WhaleSessionCache::FarbleAudioChannel(float* dst, size_t count) {
  if (audio_farbling_helper_) {
    audio_farbling_helper_->FarbleAudioChannel(dst, count);
  }
}

void WhaleSessionCache::PerturbPixels(const unsigned char* data, size_t size) {
  if (!farbling_enabled_ || farbling_level_ == WhaleFarblingLevel::OFF) {
    return;
  }
  PerturbPixelsInternal(data, size);
}

void WhaleSessionCache::PerturbPixelsInternal(const unsigned char* data,
                                              size_t size) {
  if (!data || size == 0) {
    return;
  }

  uint8_t* pixels = const_cast<uint8_t*>(data);
  // This needs to be type size_t because we pass it to base::StringPiece
  // later for content hashing. This is safe because the maximum canvas
  // dimensions are less than SIZE_T_MAX. (Width and height are each
  // limited to 32,767 pixels.)
  // Four bits per pixel
  const size_t pixel_count = size / 4;
  // calculate initial seed to find first pixel to perturb, based on session
  // key, domain key, and canvas contents
  crypto::HMAC h(crypto::HMAC::SHA256);
  uint64_t session_plus_domain_key =
      session_key_ ^ *reinterpret_cast<uint64_t*>(domain_key_);
  CHECK(h.Init(reinterpret_cast<const unsigned char*>(&session_plus_domain_key),
               sizeof session_plus_domain_key));
  uint8_t canvas_key[32];
  CHECK(h.Sign(base::StringPiece(reinterpret_cast<const char*>(pixels), size),
               canvas_key, sizeof canvas_key));
  uint64_t v = *reinterpret_cast<uint64_t*>(canvas_key);
  uint64_t pixel_index;
  // choose which channel (R, G, or B) to perturb
  uint8_t channel;
  // iterate through 32-byte canvas key and use each bit to determine how to
  // perturb the current pixel
  for (int i = 0; i < 32; i++) {
    uint8_t bit = canvas_key[i];
    for (int j = 0; j < 16; j++) {
      if (j % 8 == 0) {
        bit = canvas_key[i];
      }
      channel = v % 3;
      pixel_index = 4 * (v % pixel_count) + channel;
      pixels[pixel_index] = pixels[pixel_index] ^ (bit & 0x1);
      bit = bit >> 1;
      // find next pixel to perturb
      v = lfsr_next(v);
    }
  }
}

WTF::String WhaleSessionCache::GenerateRandomString(std::string seed,
                                                    wtf_size_t length) {
  uint8_t key[32];
  crypto::HMAC h(crypto::HMAC::SHA256);
  CHECK(h.Init(reinterpret_cast<const unsigned char*>(&domain_key_),
               sizeof domain_key_));
  CHECK(h.Sign(seed, key, sizeof key));
  // initial PRNG seed based on session key and passed-in seed string
  uint64_t v = *reinterpret_cast<uint64_t*>(key);
  UChar* destination;
  WTF::String value = WTF::String::CreateUninitialized(length, destination);
  for (wtf_size_t i = 0; i < length; i++) {
    destination[i] =
        kLettersForRandomStrings[v % kLettersForRandomStringsLength];
    v = lfsr_next(v);
  }
  return value;
}

WTF::String WhaleSessionCache::FarbledUserAgent(WTF::String real_user_agent) {
  FarblingPRNG prng = MakePseudoRandomGenerator();
  WTF::StringBuilder result;
  result.Append(real_user_agent);
  int extra = prng() % kFarbledUserAgentMaxExtraSpaces;
  for (int i = 0; i < extra; i++) {
    result.Append(" ");
  }
  return result.ToString();
}

int WhaleSessionCache::FarbledInteger(FarbleKey key,
                                      int spoof_value,
                                      int min_random_offset,
                                      int max_random_offset) {
  auto item = farbled_integers_.find(key);
  if (item == farbled_integers_.end()) {
    FarblingPRNG prng = MakePseudoRandomGenerator(key);
    auto added = farbled_integers_.insert(
        key, base::checked_cast<int>(
                 prng() % (1 + max_random_offset - min_random_offset) +
                 min_random_offset));

    return added.stored_value->value + spoof_value;
  }
  return item->value + spoof_value;
}

FarblingPRNG WhaleSessionCache::MakePseudoRandomGenerator(FarbleKey key) {
  uint64_t seed =
      *reinterpret_cast<uint64_t*>(domain_key_) ^ static_cast<uint64_t>(key);
  return FarblingPRNG(seed);
}

}  // namespace whale
