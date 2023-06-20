// Copyright (c) 2023 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

import * as React from 'react'
import Card from './card'

export default function AdCard() {
  return (
    <Card>
      <div style={{ height: '200px', background: 'red' }}>
        This is a super cool ad
      </div>
    </Card>
  )
}
