/* Copyright (c) 2023 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_ads/core/internal/ads/notification_ad_features.h"

namespace brave_ads::notification_ads::features {

BASE_FEATURE(kFeature, "NotificationAds", base::FEATURE_ENABLED_BY_DEFAULT);

bool IsEnabled() {
  return base::FeatureList::IsEnabled(kFeature);
}

}  // namespace brave_ads::notification_ads::features
