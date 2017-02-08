// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <memory>
#include <string>

#include "application/services/service_provider.fidl.h"
#include "lib/fidl/cpp/bindings/binding.h"
#include "lib/ftl/macros.h"

namespace netconnector {

class NetConnectorImpl;

// Provides services on a remote device.
class DeviceServiceProvider : public modular::ServiceProvider {
 public:
  static std::unique_ptr<DeviceServiceProvider> Create(
      const std::string& device_name,
      const std::string& device_address,
      uint16_t port,
      fidl::InterfaceRequest<modular::ServiceProvider> request,
      NetConnectorImpl* owner);

  ~DeviceServiceProvider() override;

  void ConnectToService(const fidl::String& service_name,
                        mx::channel channel) override;

 private:
  DeviceServiceProvider(
      const std::string& device_name,
      const std::string& device_address,
      uint16_t port,
      fidl::InterfaceRequest<modular::ServiceProvider> request,
      NetConnectorImpl* owner);

  std::string device_name_;
  std::string device_address_;
  uint16_t port_;
  fidl::Binding<modular::ServiceProvider> binding_;
  NetConnectorImpl* owner_;

  FTL_DISALLOW_COPY_AND_ASSIGN(DeviceServiceProvider);
};

}  // namespace netconnector
