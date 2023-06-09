/* Copyright (c) 2023 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#import "chrome/browser/app_controller_mac.h"

#define dealloc                          \
  dummy{} - (instancetype)initForBrave { \
    return [self init];                  \
  }                                      \
  -(void)dealloc

#include "src/chrome/browser/app_controller_mac.mm"
#undef dealloc
