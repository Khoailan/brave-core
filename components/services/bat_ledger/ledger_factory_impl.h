/* Copyright (c) 2023 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_COMPONENTS_SERVICES_BAT_LEDGER_LEDGER_FACTORY_IMPL_H_
#define BRAVE_COMPONENTS_SERVICES_BAT_LEDGER_LEDGER_FACTORY_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "brave/components/services/bat_ledger/public/interfaces/ledger_factory.mojom.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace brave_rewards::internal {

class LedgerFactoryImpl : public mojom::LedgerFactory {
 public:
  explicit LedgerFactoryImpl(
      mojo::PendingReceiver<mojom::LedgerFactory> receiver);

  ~LedgerFactoryImpl() override;

  LedgerFactoryImpl(const LedgerFactoryImpl&) = delete;
  LedgerFactoryImpl& operator=(const LedgerFactoryImpl&) = delete;

  void CreateLedger(
      mojo::PendingAssociatedReceiver<mojom::Ledger> ledger_receiver,
      mojo::PendingAssociatedRemote<mojom::LedgerClient> ledger_client_remote,
      CreateLedgerCallback callback) override;

 private:
  void OnLedgerCreated(CreateLedgerCallback callback);

  mojo::Receiver<mojom::LedgerFactory> receiver_;
  base::WeakPtrFactory<LedgerFactoryImpl> weak_factory_{this};
};

}  // namespace brave_rewards::internal

#endif  // BRAVE_COMPONENTS_SERVICES_BAT_LEDGER_LEDGER_FACTORY_IMPL_H_
