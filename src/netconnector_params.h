// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <string>
#include <unordered_map>

#include "application/services/application_launcher.fidl.h"
#include "apps/netconnector/src/ip_address.h"
#include "lib/ftl/command_line.h"
#include "lib/ftl/macros.h"

namespace netconnector {

class NetConnectorParams {
 public:
  NetConnectorParams(const ftl::CommandLine& command_line);

  bool is_valid() const { return is_valid_; }

  bool listen() const { return listen_; }

  std::unordered_map<std::string, app::ApplicationLaunchInfoPtr>
  MoveServices() {
    return std::move(launch_infos_by_service_name_);
  }

  const std::unordered_map<std::string, IpAddress>& devices() {
    return device_addresses_by_name_;
  }

  void RegisterDevice(const std::string& name, const IpAddress& address);

 private:
  void Usage();

  bool ReadConfigFrom(const std::string& config_file);

  bool ParseConfig(const std::string& string);

  void RegisterService(const std::string& selector,
                       app::ApplicationLaunchInfoPtr launch_info);

  bool is_valid_;
  bool listen_ = false;
  std::unordered_map<std::string, app::ApplicationLaunchInfoPtr>
      launch_infos_by_service_name_;
  std::unordered_map<std::string, IpAddress> device_addresses_by_name_;

  FTL_DISALLOW_COPY_AND_ASSIGN(NetConnectorParams);
};

}  // namespace netconnector
