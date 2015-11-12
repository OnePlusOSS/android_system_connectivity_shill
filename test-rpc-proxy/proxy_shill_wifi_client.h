//
// Copyright (C) 2015 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef PROXY_SHILL_WIFI_CLIENT_H
#define PROXY_SHILL_WIFI_CLIENT_H

#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>

#include <string>

#include <brillo/any.h>
#include <brillo/variant_dictionary.h>
// Abstract class which defines the interface for the RPC server to talk to Shill.
// This helps in abstracting out the underlying protocol that Shill client
// needs to use: Dbus, Binder, etc.
// TODO: Need to come up with comments explaining what each method needs to do here.
class ProxyShillWifiClient {
 public:
  enum AutoConnectType {
    kAutoConnectTypeDisabled = 0,
    kAutoConnectTypeEnabled = 1,
    kAutoConnectTypeUnspecified
  };
  enum StationType {
    kStationTypeIBSS,
    kStationTypeManaged,
    kStationTypeUnknown,
    kStationTypeDefault = kStationTypeManaged
  };

  ProxyShillWifiClient() = default;
  virtual ~ProxyShillWifiClient() = default;
  virtual bool SetLogging() = 0;
  virtual bool RemoveAllWifiEntries() = 0;
  virtual bool ConfigureServiceByGuid(const std::string& guid,
                                      AutoConnectType autoconnect,
                                      const std::string& passphrase) = 0;
  virtual bool ConfigureWifiService(const std::string& ssid,
                                    const std::string& security,
                                    const brillo::VariantDictionary& security_params,
                                    bool save_credentials,
                                    StationType station_type,
                                    bool hidden_network,
                                    const std::string& guid,
                                    AutoConnectType autoconnect) = 0;
  virtual bool ConnectToWifiNetwork(const std::string& ssid,
                                    const std::string& security,
                                    const brillo::VariantDictionary& security_params,
                                    bool save_credentials,
                                    StationType station_type,
                                    bool hidden_network,
                                    const std::string& guid,
                                    AutoConnectType autoconnect,
                                    long discovery_timeout_milliseconds,
                                    long association_timeout_milliseconds,
                                    long configuration_timeout_milliseconds,
                                    long* discovery_time_milliseconds,
                                    long* association_time_milliseconds,
                                    long* configuration_time_milliseconds,
                                    std::string* failure_reason) = 0;
  virtual bool DisconnectFromWifiNetwork(const std::string& ssid,
                                         long disconnect_timeout_milliseconds,
                                         long* disconnect_time_milliseconds,
                                         std::string* failure_reason) = 0;
  virtual bool ConfigureBgScan(const std::string& interface_name,
                               const std::string& method_name,
                               uint16 short_interval,
                               uint16 long_interval,
                               int signal_threshold) = 0;
  virtual bool GetActiveWifiSsids(std::vector<std::string>* ssids) = 0;
  virtual bool WaitForServiceStates(const std::string& ssid,
                                    const std::vector<std::string>& expected_states,
                                    long wait_timeout_milliseconds,
                                    std::string* final_state,
                                    long* wait_time_milliseconds) = 0;
  virtual bool CreateProfile(const std::string& profile_name) = 0;
  virtual bool PushProfile(const std::string& profile_name) = 0;
  virtual bool PopProfile(const std::string& profile_name) = 0;
  virtual bool RemoveProfile(const std::string& profile_name) = 0;
  virtual bool CleanProfiles() = 0;
  virtual bool DeleteEntriesForSsid(std::string ssid) = 0;
  virtual std::vector<std::string> ListControlledWifiInterfaces() = 0;
  virtual bool Disconnect(std::string ssid) = 0;
  virtual std::string GetServiceOrder() = 0;
  virtual bool SetServiceOrder(std::string service_order) = 0;
  virtual bool SetSchedScan(bool enable) = 0;
  virtual std::string GetPropertyOnDevice(std::string interface_name,
                                          std::string property_name) = 0;
  virtual bool SetPropertyOnDevice(std::string interface_name,
                                   std::string property_name,
                                   std::string property_value) = 0;
  virtual bool RequestRoam(std::string bssid, std::string interface_name) = 0;
  virtual bool SetDeviceEnabled(std::string interface_name, bool enable) = 0;
  virtual bool DiscoverTDLSLink(std::string interface_name,
                                std::string peer_mac_address) = 0;
  virtual bool EstablishTDLSLink(std::string interface_name,
                                 std::string peer_mac_address) = 0;
  virtual bool QueryTDLSLink(std::string interface_name,
                             std::string peer_mac_address) = 0;
  virtual bool AddWakePacketSource(std::string interface_name,
                                   std::string source_ip_address) = 0;
  virtual bool RemoveWakePacketSource(std::string interface_name,
                                      std::string source_ip_address) = 0;
  virtual bool RemoveAllWakePacketSources(std::string interface_name) = 0;

  std::string GetModeFromStationType(StationType station_type);
};

#endif // PROXY_SHILL_WIFI_CLIENT_H
