/* Copyright (c) 2023 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_MISC_METRICS_PROCESS_MISC_METRICS_H_
#define BRAVE_BROWSER_MISC_METRICS_PROCESS_MISC_METRICS_H_

#include <memory>

class PrefRegistrySimple;
class PrefService;

namespace misc_metrics {

class MenuMetrics;

class ProcessMiscMetrics {
 public:
  explicit ProcessMiscMetrics(PrefService* local_state);
  ~ProcessMiscMetrics();

  ProcessMiscMetrics(const ProcessMiscMetrics&) = delete;
  ProcessMiscMetrics& operator=(const ProcessMiscMetrics&) = delete;

  static void RegisterPrefs(PrefRegistrySimple* registry);

  MenuMetrics* menu_metrics();

 private:
  std::unique_ptr<MenuMetrics> menu_metrics_;
};

}  // namespace misc_metrics

#endif  // BRAVE_BROWSER_MISC_METRICS_PROCESS_MISC_METRICS_H_
