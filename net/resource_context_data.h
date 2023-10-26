/* Copyright 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
// Copyright (c) 2023 NAVER Corp. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WHALE_WHALE_BROWSER_NET_RESOURCE_CONTEXT_DATA_H_
#define WHALE_WHALE_BROWSER_NET_RESOURCE_CONTEXT_DATA_H_

#include <memory>
#include <set>
#include <string>

#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/ref_counted.h"
#include "base/supports_user_data.h"
#include "content/public/browser/content_browser_client.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "whale/whale/browser/net/whale_proxying_url_loader_factory.h"

// Owns proxying factories for URLLoaders and websocket proxies. There is
// one |ResourceContextData| per profile.
class ResourceContextData : public base::SupportsUserData::Data {
 public:
  ResourceContextData(const ResourceContextData&) = delete;
  ResourceContextData& operator=(const ResourceContextData&) = delete;
  ~ResourceContextData() override;

  static void StartProxying(
      content::BrowserContext* browser_context,
      int render_process_id,
      int frame_tree_node_id,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory);

  void RemoveProxy(WhaleProxyingURLLoaderFactory* proxy);
  uint64_t next_request_id() { return ++request_id_; }

 private:
  ResourceContextData();

  uint64_t request_id_ = 0;

  std::set<std::unique_ptr<WhaleProxyingURLLoaderFactory>,
           base::UniquePtrComparator>
      proxies_;

  base::WeakPtrFactory<ResourceContextData> weak_factory_;
};

#endif  // WHALE_WHALE_BROWSER_NET_RESOURCE_CONTEXT_DATA_H_
