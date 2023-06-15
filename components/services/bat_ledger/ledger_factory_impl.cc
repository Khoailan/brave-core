/* Copyright (c) 2023 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/components/services/bat_ledger/ledger_factory_impl.h"

#include <memory>
#include <utility>

#include "base/task/thread_pool.h"
#include "brave/components/brave_rewards/core/ledger_impl.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace brave_rewards::internal {

namespace {

void CreateLedgerOnSequence(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    mojo::PendingReceiver<mojom::Ledger> receiver,
    mojo::PendingRemote<mojom::LedgerClient> client_remote) {
  auto instance = std::make_unique<LedgerImpl>(std::move(client_remote));
  // A self-owner receiver will delete the instance when the pipe is destroyed.
  mojo::MakeSelfOwnedReceiver(std::move(instance), std::move(receiver),
                              task_runner);
}

}  // namespace

mojo::Remote<mojom::LedgerFactory>
LedgerFactoryImpl::CreateSelfOwnedReceiver() {
  mojo::Remote<mojom::LedgerFactory> remote;
  mojo::MakeSelfOwnedReceiver(std::make_unique<LedgerFactoryImpl>(),
                              remote.BindNewPipeAndPassReceiver());
  return remote;
}

LedgerFactoryImpl::LedgerFactoryImpl() : receiver_(this) {}

LedgerFactoryImpl::LedgerFactoryImpl(
    mojo::PendingReceiver<mojom::LedgerFactory> receiver)
    : receiver_(this, std::move(receiver)) {}

LedgerFactoryImpl::~LedgerFactoryImpl() {
  LOG(ERROR) << "LedgerFactory destructor";
}

void LedgerFactoryImpl::CreateLedger(
    mojo::PendingReceiver<mojom::Ledger> ledger_receiver,
    mojo::PendingRemote<mojom::LedgerClient> ledger_client_remote,
    CreateLedgerCallback callback) {
  // Ledger instances must be created on their own unique sequences because they
  // take advantage of sequence-local state. Create a new sequenced task runner
  // (which creates a new sequence) and create the instance on that sequence.
  // Since the Ledger interface includes [Sync] methods, we must specify
  // `base::WithBaseSyncPrimitives`.
  auto task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::WithBaseSyncPrimitives(),
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN});

  task_runner->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(CreateLedgerOnSequence, task_runner,
                     std::move(ledger_receiver),
                     std::move(ledger_client_remote)),
      base::BindOnce(&LedgerFactoryImpl::OnLedgerCreated,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void LedgerFactoryImpl::OnLedgerCreated(CreateLedgerCallback callback) {
  std::move(callback).Run();
}

}  // namespace brave_rewards::internal
