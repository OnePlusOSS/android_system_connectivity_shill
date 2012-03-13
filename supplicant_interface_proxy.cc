// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/supplicant_interface_proxy.h"

#include <map>
#include <string>

#include <base/logging.h>
#include <dbus-c++/dbus.h>

#include "shill/wifi.h"

using std::map;
using std::string;

namespace shill {

SupplicantInterfaceProxy::SupplicantInterfaceProxy(
    const WiFiRefPtr &wifi,
    DBus::Connection *bus,
    const ::DBus::Path &object_path,
    const char *dbus_addr)
    : proxy_(wifi, bus, object_path, dbus_addr) {}

SupplicantInterfaceProxy::~SupplicantInterfaceProxy() {}

::DBus::Path SupplicantInterfaceProxy::AddNetwork(
    const std::map<std::string, ::DBus::Variant> &args) {
  return proxy_.AddNetwork(args);
}

void SupplicantInterfaceProxy::ClearCachedCredentials() {
  return proxy_.ClearCachedCredentials();
}

void SupplicantInterfaceProxy::Disconnect() {
  return proxy_.Disconnect();
}

void SupplicantInterfaceProxy::FlushBSS(const uint32_t &age) {
  return proxy_.FlushBSS(age);
}

void SupplicantInterfaceProxy::RemoveAllNetworks() {
  return proxy_.RemoveAllNetworks();
}

void SupplicantInterfaceProxy::RemoveNetwork(const ::DBus::Path &network) {
  return proxy_.RemoveNetwork(network);
}

void SupplicantInterfaceProxy::Scan(
    const std::map<std::string, ::DBus::Variant> &args) {
  return proxy_.Scan(args);
}

void SupplicantInterfaceProxy::SelectNetwork(const ::DBus::Path &network) {
  return proxy_.SelectNetwork(network);
}

void SupplicantInterfaceProxy::SetFastReauth(bool enabled) {
  return proxy_.FastReauth(enabled);
}

void SupplicantInterfaceProxy::SetScanInterval(int32 scan_interval) {
  return proxy_.ScanInterval(scan_interval);
}

// definitions for private class SupplicantInterfaceProxy::Proxy

SupplicantInterfaceProxy::Proxy::Proxy(
    const WiFiRefPtr &wifi, DBus::Connection *bus,
    const DBus::Path &dbus_path, const char *dbus_addr)
    : DBus::ObjectProxy(*bus, dbus_path, dbus_addr),
      wifi_(wifi) {}

SupplicantInterfaceProxy::Proxy::~Proxy() {}

void SupplicantInterfaceProxy::Proxy::BlobAdded(const string &/*blobname*/) {
  LOG(INFO) << __func__;
  // XXX
}

void SupplicantInterfaceProxy::Proxy::BlobRemoved(const string &/*blobname*/) {
  LOG(INFO) << __func__;
  // XXX
}

void SupplicantInterfaceProxy::Proxy::BSSAdded(
    const ::DBus::Path &BSS,
    const std::map<string, ::DBus::Variant> &properties) {
  LOG(INFO) << __func__;
  wifi_->BSSAdded(BSS, properties);
}

void SupplicantInterfaceProxy::Proxy::BSSRemoved(const ::DBus::Path &BSS) {
  LOG(INFO) << __func__;
  wifi_->BSSRemoved(BSS);
}

void SupplicantInterfaceProxy::Proxy::NetworkAdded(
    const ::DBus::Path &/*network*/,
    const std::map<string, ::DBus::Variant> &/*properties*/) {
  LOG(INFO) << __func__;
  // XXX
}

void SupplicantInterfaceProxy::Proxy::NetworkRemoved(
    const ::DBus::Path &/*network*/) {
  LOG(INFO) << __func__;
  // TODO(quiche): Pass this up to WiFi, so that it can clean its
  // rpcid_by_service_ map. crosbug.com/24699
}

void SupplicantInterfaceProxy::Proxy::NetworkSelected(
    const ::DBus::Path &/*network*/) {
  LOG(INFO) << __func__;
  // XXX
}

void SupplicantInterfaceProxy::Proxy::PropertiesChanged(
    const std::map<string, ::DBus::Variant> &properties) {
  LOG(INFO) << __func__;
  wifi_->PropertiesChanged(properties);
}

void SupplicantInterfaceProxy::Proxy::ScanDone(const bool& success) {
  LOG(INFO) << __func__ << " " << success;
  if (success) {
    wifi_->ScanDone();
  }
}

}  // namespace shill
