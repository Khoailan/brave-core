/* Copyright (c) 2023 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_COMPONENTS_BRAVE_VPN_BROWSER_CONNECTION_WIREGUARD_WIN_BRAVE_VPN_WIREGUARD_SERVICE_STATUS_TRAY_STATUS_TRAY_RUNNER_H_
#define BRAVE_COMPONENTS_BRAVE_VPN_BROWSER_CONNECTION_WIREGUARD_WIN_BRAVE_VPN_WIREGUARD_SERVICE_STATUS_TRAY_STATUS_TRAY_RUNNER_H_

#include <memory>

#include "base/no_destructor.h"
#include "base/win/registry.h"
#include "base/win/windows_types.h"
#include "brave/components/brave_vpn/browser/connection/common/win/brave_windows_service_watcher.h"
#include "brave/components/brave_vpn/browser/connection/wireguard/win/brave_vpn_wireguard_service/status_tray/status_icon/tray_menu_model.h"

namespace ui {
class SimpleMenuModel;
}  // namespace ui

namespace brave_vpn {

class StatusTray;

class StatusTrayRunner : public TrayMenuModel::Delegate {
 public:
  static StatusTrayRunner* GetInstance();

  StatusTrayRunner(const StatusTrayRunner&) = delete;
  StatusTrayRunner& operator=(const StatusTrayRunner&) = delete;

  void SetupStatusIcon();

  HRESULT Run();
  void SignalExit();

  // TrayMenuModel::Delegate
  void ExecuteCommand(int command_id, int event_flags) override;
  void OnMenuWillShow(ui::SimpleMenuModel* source) override;

 private:
  friend class base::NoDestructor<StatusTrayRunner>;

  StatusTrayRunner();
  ~StatusTrayRunner() override;

  void UpdateIconState(bool error);
  void OnConnected(bool success);
  void OnDisconnected(bool success);

  std::unique_ptr<StatusTray> status_tray_;
  base::OnceClosure quit_;
  base::WeakPtrFactory<StatusTrayRunner> weak_factory_{this};
};

}  // namespace brave_vpn

#endif  // BRAVE_COMPONENTS_BRAVE_VPN_BROWSER_CONNECTION_WIREGUARD_WIN_BRAVE_VPN_WIREGUARD_SERVICE_STATUS_TRAY_STATUS_TRAY_RUNNER_H_
