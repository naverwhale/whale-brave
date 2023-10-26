/* Copyright 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "whale/whale/browser/net/resource_context_data.h"

#include <memory>
#include <string>
#include <utility>

#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "net/cookies/site_for_cookies.h"

// User data key for ResourceContextData.
const void* const kResourceContextUserDataKey = &kResourceContextUserDataKey;

ResourceContextData::ResourceContextData() : weak_factory_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

ResourceContextData::~ResourceContextData() = default;

// static
void ResourceContextData::StartProxying(
    content::BrowserContext* browser_context,
    int render_process_id,
    int frame_tree_node_id,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto* self = static_cast<ResourceContextData*>(
      browser_context->GetUserData(kResourceContextUserDataKey));
  if (!self) {
    self = new ResourceContextData();
    browser_context->SetUserData(kResourceContextUserDataKey,
                                 base::WrapUnique(self));
  }

  auto proxy = std::make_unique<WhaleProxyingURLLoaderFactory>(
      browser_context, render_process_id, frame_tree_node_id,
      std::move(receiver), std::move(target_factory), self->next_request_id(),
      base::BindOnce(&ResourceContextData::RemoveProxy,
                     self->weak_factory_.GetWeakPtr()));

  self->proxies_.emplace(std::move(proxy));
}

void ResourceContextData::RemoveProxy(WhaleProxyingURLLoaderFactory* proxy) {
  auto it = proxies_.find(proxy);
  DCHECK(it != proxies_.end());
  proxies_.erase(it);
}
