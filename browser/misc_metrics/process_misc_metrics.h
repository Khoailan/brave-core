/* Copyright (c) 2023 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_MISC_METRICS_PROCESS_MISC_METRICS_H_
#define BRAVE_BROWSER_MISC_METRICS_PROCESS_MISC_METRICS_H_

#include <memory>

#include "build/build_config.h"

class PrefRegistrySimple;
class PrefService;

namespace misc_metrics {

class MenuMetrics;
#if !BUILDFLAG(IS_ANDROID)
class VerticalTabMetrics;
#endif  // !BUILDFLAG(IS_ANDROID)

class ProcessMiscMetrics {
 public:
  explicit ProcessMiscMetrics(PrefService* local_state);
  ~ProcessMiscMetrics();

  ProcessMiscMetrics(const ProcessMiscMetrics&) = delete;
  ProcessMiscMetrics& operator=(const ProcessMiscMetrics&) = delete;

  static void RegisterPrefs(PrefRegistrySimple* registry);

  MenuMetrics* menu_metrics();
#if !BUILDFLAG(IS_ANDROID)
  VerticalTabMetrics* vertical_tab_metrics();
#endif  // !BUILDFLAG(IS_ANDROID)

 private:
  std::unique_ptr<MenuMetrics> menu_metrics_;
#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<VerticalTabMetrics> vertical_tab_metrics_;
#endif  // !BUILDFLAG(IS_ANDROID)
};

}  // namespace misc_metrics

#endif  // BRAVE_BROWSER_MISC_METRICS_PROCESS_MISC_METRICS_H_
