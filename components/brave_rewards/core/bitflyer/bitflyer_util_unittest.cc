/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <string>
#include <tuple>
#include <utility>

#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "brave/components/brave_rewards/core/bitflyer/bitflyer.h"
#include "brave/components/brave_rewards/core/bitflyer/bitflyer_util.h"
#include "brave/components/brave_rewards/core/buildflags.h"
#include "brave/components/brave_rewards/core/common/random_util.h"
#include "brave/components/brave_rewards/core/ledger_callbacks.h"
#include "brave/components/brave_rewards/core/ledger_client_mock.h"
#include "brave/components/brave_rewards/core/ledger_impl_mock.h"
#include "brave/components/brave_rewards/core/state/state_keys.h"
#include "brave/components/brave_rewards/core/test/bat_ledger_test.h"
#include "testing/gmock/include/gmock/gmock.h"

// npm run test -- brave_unit_tests --filter=*BitflyerUtilTest.*

using ::testing::_;
using ::testing::Combine;
using ::testing::Values;
using ::testing::WithParamInterface;

namespace brave_rewards::internal::bitflyer {

class BitflyerUtilTest
    : public BATLedgerTest,
      public WithParamInterface<
          std::tuple<mojom::Environment, mojom::WalletStatus>> {};

TEST_F(BitflyerUtilTest, GetClientId) {
  GetLedgerImpl()->SetEnvironment(mojom::Environment::PRODUCTION);
  EXPECT_EQ(GetClientId(), BUILDFLAG(BITFLYER_PRODUCTION_CLIENT_ID));

  GetLedgerImpl()->SetEnvironment(mojom::Environment::STAGING);
  EXPECT_EQ(GetClientId(), BUILDFLAG(BITFLYER_SANDBOX_CLIENT_ID));

  GetLedgerImpl()->SetEnvironment(mojom::Environment::DEVELOPMENT);
  EXPECT_EQ(GetClientId(), BUILDFLAG(BITFLYER_SANDBOX_CLIENT_ID));
}

TEST_F(BitflyerUtilTest, GetClientSecret) {
  GetLedgerImpl()->SetEnvironment(mojom::Environment::PRODUCTION);
  EXPECT_EQ(GetClientSecret(), BUILDFLAG(BITFLYER_PRODUCTION_CLIENT_SECRET));

  GetLedgerImpl()->SetEnvironment(mojom::Environment::STAGING);
  EXPECT_EQ(GetClientSecret(), BUILDFLAG(BITFLYER_SANDBOX_CLIENT_SECRET));

  GetLedgerImpl()->SetEnvironment(mojom::Environment::DEVELOPMENT);
  EXPECT_EQ(GetClientSecret(), BUILDFLAG(BITFLYER_SANDBOX_CLIENT_SECRET));
}

TEST_F(BitflyerUtilTest, GetFeeAddress) {
  GetLedgerImpl()->SetEnvironment(mojom::Environment::PRODUCTION);
  EXPECT_EQ(GetFeeAddress(), BUILDFLAG(BITFLYER_PRODUCTION_FEE_ADDRESS));

  GetLedgerImpl()->SetEnvironment(mojom::Environment::STAGING);
  EXPECT_EQ(GetFeeAddress(), BUILDFLAG(BITFLYER_SANDBOX_FEE_ADDRESS));

  GetLedgerImpl()->SetEnvironment(mojom::Environment::DEVELOPMENT);
  EXPECT_EQ(GetFeeAddress(), BUILDFLAG(BITFLYER_SANDBOX_FEE_ADDRESS));
}

TEST_F(BitflyerUtilTest, GetServerUrl) {
  GetLedgerImpl()->SetEnvironment(mojom::Environment::PRODUCTION);
  EXPECT_EQ(endpoint::bitflyer::GetServerUrl("/test"),
            base::StrCat({BUILDFLAG(BITFLYER_PRODUCTION_URL), "/test"}));

  GetLedgerImpl()->SetEnvironment(mojom::Environment::STAGING);
  EXPECT_EQ(endpoint::bitflyer::GetServerUrl("/test"),
            base::StrCat({BUILDFLAG(BITFLYER_SANDBOX_URL), "/test"}));

  GetLedgerImpl()->SetEnvironment(mojom::Environment::DEVELOPMENT);
  EXPECT_EQ(endpoint::bitflyer::GetServerUrl("/test"),
            base::StrCat({BUILDFLAG(BITFLYER_SANDBOX_URL), "/test"}));
}

TEST_F(BitflyerUtilTest, GetWallet) {
  base::test::TaskEnvironment task_environment_;
  MockLedgerImpl mock_ledger_impl_;

  // no wallet
  EXPECT_CALL(*mock_ledger_impl_.mock_client(),
              GetStringState(state::kWalletBitflyer, _))
      .Times(1)
      .WillOnce([](const std::string&, auto callback) {
        std::move(callback).Run("");
      });
  auto result = mock_ledger_impl_.bitflyer()->GetWallet();
  EXPECT_FALSE(result);

  EXPECT_CALL(*mock_ledger_impl_.mock_client(),
              GetStringState(state::kWalletBitflyer, _))
      .Times(1)
      .WillOnce([](const std::string&, auto callback) {
        std::string wallet = FakeEncryption::Base64EncryptString(R"({
          "account_url": "https://bitflyer.com/ex/Home?login=1",
          "address": "2323dff2ba-d0d1-4dfw-8e56-a2605bcaf4af",
          "fees": {},
          "login_url": "https://sandbox.bitflyer.com/authorize/4c2b665ca060d",
          "one_time_string": "1F747AE0A708E47ED7E650BF1856B5A4EF7E36833BDB115",
          "code_verifier": "1234567890",
          "status": 2,
          "token": "4c80232r219c30cdf112208890a32c7e00",
          "user_name": "test"
        })");
        std::move(callback).Run(std::move(wallet));
      });

  // Bitflyer wallet
  result = mock_ledger_impl_.bitflyer()->GetWallet();
  EXPECT_TRUE(result);
  EXPECT_EQ(result->address, "2323dff2ba-d0d1-4dfw-8e56-a2605bcaf4af");
  EXPECT_EQ(result->user_name, "test");
  EXPECT_EQ(result->token, "4c80232r219c30cdf112208890a32c7e00");
  EXPECT_EQ(result->status, mojom::WalletStatus::kConnected);

  task_environment_.RunUntilIdle();
}

TEST_F(BitflyerUtilTest, GenerateRandomHexStringForTesting) {
  GetLedgerImpl()->SetTesting();
  auto result = util::GenerateRandomHexString();
  EXPECT_EQ(result, "123456789");
}

TEST_F(BitflyerUtilTest, GenerateRandomHexString) {
  auto result = util::GenerateRandomHexString();
  EXPECT_EQ(result.length(), 64u);
}

INSTANTIATE_TEST_SUITE_P(GenerateLinks,
                         BitflyerUtilTest,
                         Combine(Values(mojom::Environment::PRODUCTION,
                                        mojom::Environment::STAGING,
                                        mojom::Environment::DEVELOPMENT),
                                 Values(mojom::WalletStatus::kNotConnected,
                                        mojom::WalletStatus::kConnected,
                                        mojom::WalletStatus::kLoggedOut)),
                         [](const auto& info) {
                           return (std::ostringstream()
                                   << std::get<0>(info.param) << '_'
                                   << std::get<1>(info.param))
                               .str();
                         });

TEST_P(BitflyerUtilTest, Paths) {
  const auto [environment, wallet_status] = GetParam();

  GetLedgerImpl()->SetEnvironment(environment);
  auto wallet = mojom::ExternalWallet::New();
  wallet->status = wallet_status;
  wallet->one_time_string = "one_time_string";
  wallet->code_verifier = "code_verifier";

  const auto account_url =
      base::StrCat({environment == mojom::Environment::PRODUCTION
                        ? BUILDFLAG(BITFLYER_PRODUCTION_URL)
                        : BUILDFLAG(BITFLYER_SANDBOX_URL),
                    "/ex/Home?login=1"});

  const auto activity_url =
      wallet->status == mojom::WalletStatus::kConnected
          ? base::StrCat({environment == mojom::Environment::PRODUCTION
                              ? BUILDFLAG(BITFLYER_PRODUCTION_URL)
                              : BUILDFLAG(BITFLYER_SANDBOX_URL),
                          "/ja-jp/ex/tradehistory"})
          : "";

  const auto login_url = base::StringPrintf(
      "%s/ex/OAuth/authorize"
      "?client_id=%s"
      "&scope="
      "assets "
      "create_deposit_id "
      "withdraw_to_deposit_id"
      "&redirect_uri=rewards://bitflyer/authorization"
      "&state=one_time_string"
      "&response_type=code"
      "&code_challenge_method=S256"
      "&code_challenge=73oehA2tBul5grZPhXUGQwNAjxh69zNES8bu2bVD0EM",
      environment == mojom::Environment::PRODUCTION
          ? BUILDFLAG(BITFLYER_PRODUCTION_URL)
          : BUILDFLAG(BITFLYER_SANDBOX_URL),
      environment == mojom::Environment::PRODUCTION
          ? BUILDFLAG(BITFLYER_PRODUCTION_CLIENT_ID)
          : BUILDFLAG(BITFLYER_SANDBOX_CLIENT_ID));

  EXPECT_FALSE(bitflyer::GenerateLinks(nullptr));
  const auto result = GenerateLinks(std::move(wallet));
  EXPECT_TRUE(result);
  EXPECT_EQ(result->account_url, account_url);
  EXPECT_EQ(result->activity_url, activity_url);
  EXPECT_EQ(result->login_url, login_url);
}

}  // namespace brave_rewards::internal::bitflyer
