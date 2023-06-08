/* Copyright (c) 2023 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/misc_metrics/process_misc_metrics.h"

#if !BUILDFLAG(IS_ANDROID)
#include "brave/browser/misc_metrics/vertical_tab_metrics.h"
#endif  // !BUILDFLAG(IS_ANDROID)

#include "brave/components/misc_metrics/menu_metrics.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace misc_metrics {

ProcessMiscMetrics::ProcessMiscMetrics(PrefService* local_state) {
#if !BUILDFLAG(IS_ANDROID)
  menu_metrics_ = std::make_unique<MenuMetrics>(local_state);
  vertical_tab_metrics_ = std::make_unique<VerticalTabMetrics>(local_state);
#endif
}

ProcessMiscMetrics::~ProcessMiscMetrics() = default;

MenuMetrics* ProcessMiscMetrics::menu_metrics() {
  return menu_metrics_.get();
}

#if !BUILDFLAG(IS_ANDROID)
VerticalTabMetrics* ProcessMiscMetrics::vertical_tab_metrics() {
  return vertical_tab_metrics_.get();
}
#endif

void ProcessMiscMetrics::RegisterPrefs(PrefRegistrySimple* registry) {
#if !BUILDFLAG(IS_ANDROID)
  MenuMetrics::RegisterPrefs(registry);
  VerticalTabMetrics::RegisterPrefs(registry);
#endif
}

}  // namespace misc_metrics
