/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_rewards/core/logging/logging.h"

#include "brave/components/brave_rewards/core/ledger_impl.h"

namespace brave_rewards::internal {

void Log(const char* file,
         const int line,
         const int verbose_level,
         const std::string& message) {
  LedgerImpl::GetForCurrentSequence().client()->Log(file, line, verbose_level,
                                                    message);
}

}  // namespace brave_rewards::internal
