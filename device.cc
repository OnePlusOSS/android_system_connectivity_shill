// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/device.h"

#include <netinet/in.h>
#include <linux/if.h>  // Needs definitions from netinet/in.h
#include <stdio.h>
#include <time.h>

#include <string>
#include <vector>

#include <base/bind.h>
#include <base/file_util.h>
#include <base/memory/ref_counted.h>
#include <base/strings/stringprintf.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/async_connection.h"
#include "shill/connection.h"
#include "shill/control_interface.h"
#include "shill/device_dbus_adaptor.h"
#include "shill/dhcp_config.h"
#include "shill/dhcp_provider.h"
#include "shill/error.h"
#include "shill/event_dispatcher.h"
#include "shill/geolocation_info.h"
#include "shill/http_proxy.h"
#include "shill/ip_address.h"
#include "shill/link_monitor.h"
#include "shill/logging.h"
#include "shill/manager.h"
#include "shill/metrics.h"
#include "shill/property_accessor.h"
#include "shill/refptr_types.h"
#include "shill/rtnl_handler.h"
#include "shill/service.h"
#include "shill/socket_info_reader.h"
#include "shill/store_interface.h"
#include "shill/technology.h"
#include "shill/tethering.h"
#include "shill/traffic_monitor.h"

using base::Bind;
using base::FilePath;
using base::StringPrintf;
using std::string;
using std::vector;

namespace shill {

// static
const char Device::kIPFlagTemplate[] = "/proc/sys/net/%s/conf/%s/%s";
// static
const char Device::kIPFlagVersion4[] = "ipv4";
// static
const char Device::kIPFlagVersion6[] = "ipv6";
// static
const char Device::kIPFlagDisableIPv6[] = "disable_ipv6";
// static
const char Device::kIPFlagUseTempAddr[] = "use_tempaddr";
// static
const char Device::kIPFlagUseTempAddrUsedAndDefault[] = "2";
// static
const char Device::kIPFlagReversePathFilter[] = "rp_filter";
// static
const char Device::kIPFlagReversePathFilterEnabled[] = "1";
// static
const char Device::kIPFlagReversePathFilterLooseMode[] = "2";
// static
const char Device::kStoragePowered[] = "Powered";
// static
const char Device::kStorageReceiveByteCount[] = "ReceiveByteCount";
// static
const char Device::kStorageTransmitByteCount[] = "TransmitByteCount";
// static
const char Device::kFallbackDnsTestHostname[] = "www.gstatic.com";
// static
const char* Device::kFallbackDnsServers[] = {
    "8.8.8.8",
    "8.8.4.4"
};

// static
const int Device::kDNSTimeoutMilliseconds = 5000;

Device::Device(ControlInterface *control_interface,
               EventDispatcher *dispatcher,
               Metrics *metrics,
               Manager *manager,
               const string &link_name,
               const string &address,
               int interface_index,
               Technology::Identifier technology)
    : enabled_(false),
      enabled_persistent_(true),
      enabled_pending_(enabled_),
      reconnect_(true),
      hardware_address_(address),
      interface_index_(interface_index),
      running_(false),
      link_name_(link_name),
      unique_id_(link_name),
      control_interface_(control_interface),
      dispatcher_(dispatcher),
      metrics_(metrics),
      manager_(manager),
      weak_ptr_factory_(this),
      adaptor_(control_interface->CreateDeviceAdaptor(this)),
      portal_detector_callback_(Bind(&Device::PortalDetectorCallback,
                                     weak_ptr_factory_.GetWeakPtr())),
      fallback_dns_result_callback_(Bind(&Device::FallbackDNSResultCallback,
                                         weak_ptr_factory_.GetWeakPtr())),
      technology_(technology),
      portal_attempts_to_online_(0),
      receive_byte_offset_(0),
      transmit_byte_offset_(0),
      dhcp_provider_(DHCPProvider::GetInstance()),
      rtnl_handler_(RTNLHandler::GetInstance()) {
  store_.RegisterConstString(kAddressProperty, &hardware_address_);

  // kBgscanMethodProperty: Registered in WiFi
  // kBgscanShortIntervalProperty: Registered in WiFi
  // kBgscanSignalThresholdProperty: Registered in WiFi

  // kCellularAllowRoamingProperty: Registered in Cellular
  // kCarrierProperty: Registered in Cellular
  // kEsnProperty: Registered in Cellular
  // kHomeProviderProperty: Registered in Cellular
  // kImeiProperty: Registered in Cellular
  // kIccidProperty: Registered in Cellular
  // kImsiProperty: Registered in Cellular
  // kManufacturerProperty: Registered in Cellular
  // kMdnProperty: Registered in Cellular
  // kMeidProperty: Registered in Cellular
  // kMinProperty: Registered in Cellular
  // kModelIDProperty: Registered in Cellular
  // kFirmwareRevisionProperty: Registered in Cellular
  // kHardwareRevisionProperty: Registered in Cellular
  // kPRLVersionProperty: Registered in Cellular
  // kSIMLockStatusProperty: Registered in Cellular
  // kFoundNetworksProperty: Registered in Cellular
  // kDBusObjectProperty: Register in Cellular

  store_.RegisterConstString(kInterfaceProperty, &link_name_);
  HelpRegisterConstDerivedRpcIdentifiers(kIPConfigsProperty,
                                         &Device::AvailableIPConfigs);
  store_.RegisterConstString(kNameProperty, &link_name_);
  store_.RegisterConstBool(kPoweredProperty, &enabled_);
  HelpRegisterConstDerivedString(kTypeProperty,
                                 &Device::GetTechnologyString);
  HelpRegisterConstDerivedUint64(kLinkMonitorResponseTimeProperty,
                                 &Device::GetLinkMonitorResponseTime);

  // TODO(cmasone): Chrome doesn't use this...does anyone?
  // store_.RegisterConstBool(kReconnectProperty, &reconnect_);

  // TODO(cmasone): Figure out what shill concept maps to flimflam's "Network".
  // known_properties_.push_back(kNetworksProperty);

  // kRoamThresholdProperty: Registered in WiFi
  // kScanningProperty: Registered in WiFi, Cellular
  // kScanIntervalProperty: Registered in WiFi, Cellular

  if (manager_ && manager_->device_info()) {  // Unit tests may not have these.
    manager_->device_info()->GetByteCounts(
        interface_index_, &receive_byte_offset_, &transmit_byte_offset_);
    HelpRegisterConstDerivedUint64(kReceiveByteCountProperty,
                                   &Device::GetReceiveByteCountProperty);
    HelpRegisterConstDerivedUint64(kTransmitByteCountProperty,
                                   &Device::GetTransmitByteCountProperty);
  }

  LOG(INFO) << "Device created: " << link_name_
            << " index " << interface_index_;
}

Device::~Device() {
  LOG(INFO) << "Device destructed: " << link_name_
            << " index " << interface_index_;
}

void Device::LinkEvent(unsigned flags, unsigned change) {
  SLOG(Device, 2) << "Device " << link_name_
                  << std::showbase << std::hex
                  << " flags " << flags << " changed " << change
                  << std::dec << std::noshowbase;
}

void Device::Scan(ScanType scan_type, Error *error, const string &reason) {
  SLOG(Device, 2) << __func__ << " [Device] on " << link_name() << " from "
                  << reason;
  Error::PopulateAndLog(error, Error::kNotSupported,
                        "Device doesn't support scan.");
}

void Device::RegisterOnNetwork(const std::string &/*network_id*/, Error *error,
                                 const ResultCallback &/*callback*/) {
  Error::PopulateAndLog(error, Error::kNotSupported,
                        "Device doesn't support network registration.");
}

void Device::RequirePIN(
    const string &/*pin*/, bool /*require*/,
    Error *error, const ResultCallback &/*callback*/) {
  SLOG(Device, 2) << __func__;
  Error::PopulateAndLog(error, Error::kNotSupported,
                        "Device doesn't support RequirePIN.");
}

void Device::EnterPIN(const string &/*pin*/,
                      Error *error, const ResultCallback &/*callback*/) {
  SLOG(Device, 2) << __func__;
  Error::PopulateAndLog(error, Error::kNotSupported,
                        "Device doesn't support EnterPIN.");
}

void Device::UnblockPIN(const string &/*unblock_code*/,
                        const string &/*pin*/,
                        Error *error, const ResultCallback &/*callback*/) {
  SLOG(Device, 2) << __func__;
  Error::PopulateAndLog(error, Error::kNotSupported,
                        "Device doesn't support UnblockPIN.");
}

void Device::ChangePIN(const string &/*old_pin*/,
                       const string &/*new_pin*/,
                       Error *error, const ResultCallback &/*callback*/) {
  SLOG(Device, 2) << __func__;
  Error::PopulateAndLog(error, Error::kNotSupported,
                        "Device doesn't support ChangePIN.");
}

void Device::Reset(Error *error, const ResultCallback &/*callback*/) {
  SLOG(Device, 2) << __func__;
  Error::PopulateAndLog(error, Error::kNotSupported,
                        "Device doesn't support Reset.");
}

void Device::SetCarrier(const string &/*carrier*/,
                        Error *error, const ResultCallback &/*callback*/) {
  SLOG(Device, 2) << __func__;
  Error::PopulateAndLog(error, Error::kNotSupported,
                        "Device doesn't support SetCarrier.");
}

bool Device::IsIPv6Allowed() const {
  return true;
}

void Device::DisableIPv6() {
  SLOG(Device, 2) << __func__;
  SetIPFlag(IPAddress::kFamilyIPv6, kIPFlagDisableIPv6, "1");
}

void Device::EnableIPv6() {
  SLOG(Device, 2) << __func__;
  if (!IsIPv6Allowed()) {
    LOG(INFO) << "Skip enabling IPv6 on " << link_name_
              << " as it is not allowed.";
    return;
  }
  SetIPFlag(IPAddress::kFamilyIPv6, kIPFlagDisableIPv6, "0");
}

void Device::EnableIPv6Privacy() {
  SetIPFlag(IPAddress::kFamilyIPv6, kIPFlagUseTempAddr,
            kIPFlagUseTempAddrUsedAndDefault);
}

void Device::DisableReversePathFilter() {
  // TODO(pstew): Current kernel doesn't offer reverse-path filtering flag
  // for IPv6.  crbug.com/207193
  SetIPFlag(IPAddress::kFamilyIPv4, kIPFlagReversePathFilter,
            kIPFlagReversePathFilterLooseMode);
}

void Device::EnableReversePathFilter() {
  SetIPFlag(IPAddress::kFamilyIPv4, kIPFlagReversePathFilter,
            kIPFlagReversePathFilterEnabled);
}

bool Device::IsConnected() const {
  if (selected_service_)
    return selected_service_->IsConnected();
  return false;
}

bool Device::IsConnectedToService(const ServiceRefPtr &service) const {
  return service == selected_service_ && IsConnected();
}

bool Device::IsConnectedViaTether() const {
  return
      ipconfig_.get() &&
      ipconfig_->properties().vendor_encapsulated_options ==
          Tethering::kAndroidVendorEncapsulatedOptions;
}

string Device::GetRpcIdentifier() const {
  return adaptor_->GetRpcIdentifier();
}

string Device::GetStorageIdentifier() {
  string id = GetRpcIdentifier();
  ControlInterface::RpcIdToStorageId(&id);
  size_t needle = id.find('_');
  DLOG_IF(ERROR, needle == string::npos) << "No _ in storage id?!?!";
  id.replace(id.begin() + needle + 1, id.end(), hardware_address_);
  return id;
}

vector<GeolocationInfo> Device::GetGeolocationObjects() const {
  return vector<GeolocationInfo>();
}

string Device::GetTechnologyString(Error */*error*/) {
  return Technology::NameFromIdentifier(technology());
}

const string& Device::FriendlyName() const {
  return link_name_;
}

const string& Device::UniqueName() const {
  return unique_id_;
}

bool Device::Load(StoreInterface *storage) {
  const string id = GetStorageIdentifier();
  if (!storage->ContainsGroup(id)) {
    LOG(WARNING) << "Device is not available in the persistent store: " << id;
    return false;
  }
  enabled_persistent_ = true;
  storage->GetBool(id, kStoragePowered, &enabled_persistent_);
  uint64 rx_byte_count = 0, tx_byte_count = 0;

  manager_->device_info()->GetByteCounts(
      interface_index_, &rx_byte_count, &tx_byte_count);
  // If there is a byte-count present in the profile, the return value
  // of Device::Get*ByteCount() should be the this stored value plus
  // whatever additional bytes we receive since time-of-load.  We
  // accomplish this by the subtractions below, which can validly
  // roll over "negative" in the subtractions below and in Get*ByteCount.
  uint64 profile_byte_count;
  if (storage->GetUint64(id, kStorageReceiveByteCount, &profile_byte_count)) {
    receive_byte_offset_ = rx_byte_count - profile_byte_count;
  }
  if (storage->GetUint64(id, kStorageTransmitByteCount, &profile_byte_count)) {
    transmit_byte_offset_ = tx_byte_count - profile_byte_count;
  }

  return true;
}

bool Device::Save(StoreInterface *storage) {
  const string id = GetStorageIdentifier();
  storage->SetBool(id, kStoragePowered, enabled_persistent_);
  storage->SetUint64(id, kStorageReceiveByteCount, GetReceiveByteCount());
  storage->SetUint64(id, kStorageTransmitByteCount, GetTransmitByteCount());
  return true;
}

void Device::OnBeforeSuspend() {
  // Nothing to be done in the general case.
}

void Device::OnAfterResume() {
  if (ipconfig_) {
    SLOG(Device, 3) << "Renewing IP address on resume.";
    ipconfig_->RenewIP();
  }
  if (link_monitor_) {
    SLOG(Device, 3) << "Informing Link Monitor of resume.";
    link_monitor_->OnAfterResume();
  }
}

void Device::DropConnection() {
  SLOG(Device, 2) << __func__;
  DestroyIPConfig();
  SelectService(NULL);
}

void Device::DestroyIPConfig() {
  DisableIPv6();
  if (ipconfig_.get()) {
    ipconfig_->ReleaseIP(IPConfig::kReleaseReasonDisconnect);
    ipconfig_ = NULL;
    UpdateIPConfigsProperty();
  }
  DestroyConnection();
}

void Device::OnIPv6AddressChanged() {
  IPAddress address(IPAddress::kFamilyIPv6);
  if (!manager_->device_info()->GetPrimaryIPv6Address(
          interface_index_, &address)) {
    if (ip6config_) {
      ip6config_ = NULL;
      UpdateIPConfigsProperty();
    }
    return;
  }

  IPConfig::Properties properties;
  if (!address.IntoString(&properties.address)) {
    LOG(ERROR) << "Unable to convert IPv6 address into a string!";
    return;
  }
  properties.subnet_prefix = address.prefix();

  if (!ip6config_) {
    ip6config_ = new IPConfig(control_interface_, link_name_);
  } else if (properties.address == ip6config_->properties().address &&
             properties.subnet_prefix ==
                 ip6config_->properties().subnet_prefix) {
    SLOG(Device, 2) << __func__ << " primary address for "
                    << link_name_ << " is unchanged.";
    return;
  }

  properties.address_family = IPAddress::kFamilyIPv6;
  properties.method = kTypeIPv6;
  ip6config_->set_properties(properties);
  UpdateIPConfigsProperty();
}

bool Device::ShouldUseArpGateway() const {
  return false;
}

bool Device::IsUsingStaticIP() const {
  if (!selected_service_) {
    return false;
  }
  return selected_service_->HasStaticIPAddress();
}

bool Device::AcquireIPConfig() {
  return AcquireIPConfigWithLeaseName(string());
}

bool Device::AcquireIPConfigWithLeaseName(const string &lease_name) {
  DestroyIPConfig();
  EnableIPv6();
  bool arp_gateway = manager_->GetArpGateway() && ShouldUseArpGateway();
  ipconfig_ = dhcp_provider_->CreateConfig(link_name_,
                                           manager_->GetHostName(),
                                           lease_name,
                                           arp_gateway);
  ipconfig_->RegisterUpdateCallback(Bind(&Device::OnIPConfigUpdated,
                                         weak_ptr_factory_.GetWeakPtr()));
  ipconfig_->RegisterFailureCallback(Bind(&Device::OnIPConfigFailed,
                                          weak_ptr_factory_.GetWeakPtr()));
  ipconfig_->RegisterRefreshCallback(Bind(&Device::OnIPConfigRefreshed,
                                          weak_ptr_factory_.GetWeakPtr()));
  ipconfig_->RegisterExpireCallback(Bind(&Device::OnIPConfigExpired,
                                         weak_ptr_factory_.GetWeakPtr()));
  dispatcher_->PostTask(Bind(&Device::ConfigureStaticIPTask,
                             weak_ptr_factory_.GetWeakPtr()));
  return ipconfig_->RequestIP();
}

void Device::AssignIPConfig(const IPConfig::Properties &properties) {
  DestroyIPConfig();
  EnableIPv6();
  ipconfig_ = new IPConfig(control_interface_, link_name_);
  ipconfig_->set_properties(properties);
  dispatcher_->PostTask(Bind(&Device::OnIPConfigUpdated,
                             weak_ptr_factory_.GetWeakPtr(), ipconfig_));
}

void Device::DestroyIPConfigLease(const string &name) {
  dhcp_provider_->DestroyLease(name);
}

void Device::HelpRegisterConstDerivedString(
    const string &name,
    string(Device::*get)(Error *error)) {
  store_.RegisterDerivedString(
      name,
      StringAccessor(new CustomAccessor<Device, string>(this, get, NULL)));
}

void Device::HelpRegisterConstDerivedRpcIdentifiers(
    const string &name,
    RpcIdentifiers(Device::*get)(Error *)) {
  store_.RegisterDerivedRpcIdentifiers(
      name,
      RpcIdentifiersAccessor(
          new CustomAccessor<Device, RpcIdentifiers>(this, get, NULL)));
}

void Device::HelpRegisterConstDerivedUint64(
    const string &name,
    uint64(Device::*get)(Error *)) {
  store_.RegisterDerivedUint64(
      name,
      Uint64Accessor(
          new CustomAccessor<Device, uint64>(this, get, NULL)));
}

void Device::ConfigureStaticIPTask() {
  SLOG(Device, 2) << __func__ << " selected_service " << selected_service_.get()
                  << " ipconfig " << ipconfig_.get();

  if (!selected_service_ || !ipconfig_) {
    return;
  }

  if (IsUsingStaticIP()) {
    SLOG(Device, 2) << __func__ << " " << " configuring static IP parameters.";
    // If the parameters contain an IP address, apply them now and bring
    // the interface up.  When DHCP information arrives, it will supplement
    // the static information.
    OnIPConfigUpdated(ipconfig_);
  } else {
    // Either |ipconfig_| has just been created in AcquireIPConfig() or
    // we're being called by OnIPConfigRefreshed().  In either case a
    // DHCP client has been started, and will take care of calling
    // OnIPConfigUpdated() when it completes.
    SLOG(Device, 2) << __func__ << " " << " no static IP address.";
  }
}

void Device::OnIPConfigUpdated(const IPConfigRefPtr &ipconfig) {
  SLOG(Device, 2) << __func__;
  CreateConnection();
  if (selected_service_) {
    ipconfig->ApplyStaticIPParameters(
        selected_service_->mutable_static_ip_parameters());
    if (IsUsingStaticIP()) {
      // If we are using a statically configured IP address instead
      // of a leased IP address, release any acquired lease so it may
      // be used by others.  This allows us to merge other non-leased
      // parameters (like DNS) when they're available from a DHCP server
      // and not overridden by static parameters, but at the same time
      // we avoid taking up a dynamic IP address the DHCP server could
      // assign to someone else who might actually use it.
      ipconfig->ReleaseIP(IPConfig::kReleaseReasonStaticIP);
    }
  }
  connection_->UpdateFromIPConfig(ipconfig);
  // SetConnection must occur after the UpdateFromIPConfig so the
  // service can use the values derived from the connection.
  if (selected_service_) {
    selected_service_->SetConnection(connection_);
  }
  // The service state change needs to happen last, so that at the
  // time we report the state change to the manager, the service
  // has its connection.
  SetServiceState(Service::kStateConnected);
  OnConnected();
  portal_attempts_to_online_ = 0;
  // Subtle: Start portal detection after transitioning the service
  // to the Connected state because this call may immediately transition
  // to the Online state.
  if (selected_service_) {
    StartPortalDetection();
  }
  StartLinkMonitor();
  StartTrafficMonitor();
  UpdateIPConfigsProperty();
}

void Device::OnIPConfigFailed(const IPConfigRefPtr &ipconfig) {
  SLOG(Device, 2) << __func__;
  // TODO(pstew): This logic gets yet more complex when multiple
  // IPConfig types are run in parallel (e.g. DHCP and DHCP6)
  if (selected_service_) {
    if (IsUsingStaticIP()) {
      // Consider three cases:
      //
      // 1. We're here because DHCP failed while starting up. There
      //    are two subcases:
      //    a. DHCP has failed, and Static IP config has _not yet_
      //       completed. It's fine to do nothing, because we'll
      //       apply the static config shortly.
      //    b. DHCP has failed, and Static IP config has _already_
      //       completed. It's fine to do nothing, because we can
      //       continue to use the static config that's already
      //       been applied.
      //
      // 2. We're here because a previously valid DHCP configuration
      //    is no longer valid. There's still a static IP config,
      //    because the condition in the if clause evaluated to true.
      //    Furthermore, the static config includes an IP address for
      //    us to use.
      //
      //    The current configuration may include some DHCP
      //    parameters, overriden by any static parameters
      //    provided. We continue to use this configuration, because
      //    the only configuration element that is leased to us (IP
      //    address) will be overriden by a static parameter.
      return;
    }
  }

  ipconfig->ResetProperties();
  OnIPConfigFailure();
  UpdateIPConfigsProperty();
  DestroyConnection();
}

void Device::OnIPConfigRefreshed(const IPConfigRefPtr &ipconfig) {
  // Clear the previously applied static IP parameters.
  ipconfig->RestoreSavedIPParameters(
      selected_service_->mutable_static_ip_parameters());

  dispatcher_->PostTask(Bind(&Device::ConfigureStaticIPTask,
                             weak_ptr_factory_.GetWeakPtr()));
}

void Device::OnIPConfigFailure() {
  if (selected_service_) {
    Error error;
    selected_service_->DisconnectWithFailure(Service::kFailureDHCP, &error);
  }
}

void Device::OnIPConfigExpired(const IPConfigRefPtr &ipconfig) {
  metrics()->SendToUMA(
      metrics()->GetFullMetricName(
          Metrics::kMetricExpiredLeaseLengthSecondsSuffix, technology()),
      ipconfig->properties().lease_duration_seconds,
      Metrics::kMetricExpiredLeaseLengthSecondsMin,
      Metrics::kMetricExpiredLeaseLengthSecondsMax,
      Metrics::kMetricExpiredLeaseLengthSecondsNumBuckets);
}

void Device::OnConnected() {}

void Device::OnConnectionUpdated() {
  if (selected_service_) {
    manager_->UpdateService(selected_service_);
  }
}

void Device::CreateConnection() {
  SLOG(Device, 2) << __func__;
  if (!connection_.get()) {
    connection_ = new Connection(interface_index_,
                                 link_name_,
                                 technology_,
                                 manager_->device_info());
  }
}

void Device::DestroyConnection() {
  SLOG(Device, 2) << __func__ << " on " << link_name_;
  StopTrafficMonitor();
  StopPortalDetection();
  StopLinkMonitor();
  if (selected_service_.get()) {
    SLOG(Device, 3) << "Clearing connection of service "
                    << selected_service_->unique_name();
    selected_service_->SetConnection(NULL);
  }
  connection_ = NULL;
}

void Device::SelectService(const ServiceRefPtr &service) {
  SLOG(Device, 2) << __func__ << ": service "
                  << (service ? service->unique_name() : "*reset*")
                  << " on " << link_name_;

  if (selected_service_.get() == service.get()) {
    // No change to |selected_service_|. Return early to avoid
    // changing its state.
    return;
  }

  if (selected_service_.get()) {
    if (selected_service_->state() != Service::kStateFailure) {
      selected_service_->SetState(Service::kStateIdle);
    }
    // Just in case the Device subclass has not already done so, make
    // sure the previously selected service has its connection removed.
    selected_service_->SetConnection(NULL);
    StopTrafficMonitor();
    StopLinkMonitor();
    StopPortalDetection();
  }
  selected_service_ = service;
}

void Device::SetServiceState(Service::ConnectState state) {
  if (selected_service_.get()) {
    selected_service_->SetState(state);
  }
}

void Device::SetServiceFailure(Service::ConnectFailure failure_state) {
  if (selected_service_.get()) {
    selected_service_->SetFailure(failure_state);
  }
}

void Device::SetServiceFailureSilent(Service::ConnectFailure failure_state) {
  if (selected_service_.get()) {
    selected_service_->SetFailureSilent(failure_state);
  }
}

bool Device::SetIPFlag(IPAddress::Family family, const string &flag,
                       const string &value) {
  string ip_version;
  if (family == IPAddress::kFamilyIPv4) {
    ip_version = kIPFlagVersion4;
  } else if (family == IPAddress::kFamilyIPv6) {
    ip_version = kIPFlagVersion6;
  } else {
    NOTIMPLEMENTED();
  }
  FilePath flag_file(StringPrintf(kIPFlagTemplate, ip_version.c_str(),
                                  link_name_.c_str(), flag.c_str()));
  SLOG(Device, 2) << "Writing " << value << " to flag file "
                  << flag_file.value();
  if (base::WriteFile(flag_file, value.c_str(), value.length()) != 1) {
    LOG(ERROR) << StringPrintf("IP flag write failed: %s to %s",
                               value.c_str(), flag_file.value().c_str());
    return false;
  }
  return true;
}

string Device::PerformTDLSOperation(const string &/* operation */,
                                    const string &/* peer */,
                                    Error */* error */) {
  return "";
}

void Device::ResetByteCounters() {
  manager_->device_info()->GetByteCounts(
      interface_index_, &receive_byte_offset_, &transmit_byte_offset_);
  manager_->UpdateDevice(this);
}

bool Device::RestartPortalDetection() {
  StopPortalDetection();
  return StartPortalDetection();
}

bool Device::RequestPortalDetection() {
  if (!selected_service_) {
    SLOG(Device, 2) << FriendlyName()
            << ": No selected service, so no need for portal check.";
    return false;
  }

  if (!connection_.get()) {
    SLOG(Device, 2) << FriendlyName()
            << ": No connection, so no need for portal check.";
    return false;
  }

  if (selected_service_->state() != Service::kStatePortal) {
    SLOG(Device, 2) << FriendlyName()
            << ": Service is not in portal state.  No need to start check.";
    return false;
  }

  if (!connection_->is_default()) {
    SLOG(Device, 2) << FriendlyName()
            << ": Service is not the default connection.  Don't start check.";
    return false;
  }

  if (portal_detector_.get() && portal_detector_->IsInProgress()) {
    SLOG(Device, 2) << FriendlyName()
                    << ": Portal detection is already running.";
    return true;
  }

  return StartPortalDetection();
}

bool Device::StartPortalDetection() {
  DCHECK(selected_service_);
  if (selected_service_->IsPortalDetectionDisabled()) {
    SLOG(Device, 2) << "Service " << selected_service_->unique_name()
                    << ": Portal detection is disabled; "
                    << "marking service online.";
    SetServiceConnectedState(Service::kStateOnline);
    return false;
  }

  if (selected_service_->IsPortalDetectionAuto() &&
      !manager_->IsPortalDetectionEnabled(technology())) {
    // If portal detection is disabled for this technology, immediately set
    // the service state to "Online".
    SLOG(Device, 2) << "Device " << FriendlyName()
                    << ": Portal detection is disabled; "
                    << "marking service online.";
    SetServiceConnectedState(Service::kStateOnline);
    return false;
  }

  if (selected_service_->HasProxyConfig()) {
    // Services with HTTP proxy configurations should not be checked by the
    // connection manager, since we don't have the ability to evaluate
    // arbitrary proxy configs and their possible credentials.
    SLOG(Device, 2) << "Device " << FriendlyName()
                    << ": Service has proxy config; marking it online.";
    SetServiceConnectedState(Service::kStateOnline);
    return false;
  }

  portal_detector_.reset(new PortalDetector(connection_,
                                            dispatcher_,
                                            portal_detector_callback_));
  if (!portal_detector_->Start(manager_->GetPortalCheckURL())) {
    LOG(ERROR) << "Device " << FriendlyName()
               << ": Portal detection failed to start: likely bad URL: "
               << manager_->GetPortalCheckURL();
    SetServiceConnectedState(Service::kStateOnline);
    return false;
  }

  SLOG(Device, 2) << "Device " << FriendlyName()
                  << ": Portal detection has started.";
  return true;
}

void Device::StopPortalDetection() {
  SLOG(Device, 2) << "Device " << FriendlyName()
                  << ": Portal detection stopping.";
  portal_detector_.reset();
}

void Device::set_link_monitor(LinkMonitor *link_monitor) {
  link_monitor_.reset(link_monitor);
}

bool Device::StartLinkMonitor() {
  if (!manager_->IsTechnologyLinkMonitorEnabled(technology())) {
    SLOG(Device, 2) << "Device " << FriendlyName()
                    << ": Link Monitoring is disabled.";
    return false;
  }

  if (!link_monitor()) {
    set_link_monitor(
      new LinkMonitor(
          connection_, dispatcher_, metrics(), manager_->device_info(),
          Bind(&Device::OnLinkMonitorFailure, weak_ptr_factory_.GetWeakPtr()),
          Bind(&Device::OnLinkMonitorGatewayChange,
               weak_ptr_factory_.GetWeakPtr())));
  }

  SLOG(Device, 2) << "Device " << FriendlyName()
                  << ": Link Monitor starting.";
  return link_monitor_->Start();
}

void Device::StopLinkMonitor() {
  SLOG(Device, 2) << "Device " << FriendlyName()
                  << ": Link Monitor stopping.";
  link_monitor_.reset();
}

void Device::OnLinkMonitorFailure() {
  LOG(ERROR) << "Device " << FriendlyName()
             << ": Link Monitor indicates failure.";
}

void Device::OnLinkMonitorGatewayChange() {
  string gateway_mac = link_monitor()->gateway_mac_address().HexEncode();
  int connection_id = manager_->CalcConnectionId(
      ipconfig_->properties().gateway, gateway_mac);

  CHECK(selected_service_);
  selected_service_->set_connection_id(connection_id);

  manager_->ReportServicesOnSameNetwork(connection_id);
}

void Device::PerformFallbackDNSTest() {
  if (!fallback_dns_server_tester_.get()) {
    vector<string> dns_servers(std::begin(kFallbackDnsServers),
                               std::end(kFallbackDnsServers));
    fallback_dns_server_tester_.reset(
        new DNSServerTester(connection_,
                            dispatcher_,
                            dns_servers,
                            false,
                            fallback_dns_result_callback_));
  }

  // Perform DNS server test for Google's DNS servers.
  fallback_dns_server_tester_->Start();
}

void Device::FallbackDNSResultCallback(const DNSServerTester::Status status) {
  int result = Metrics::kFallbackDNSTestResultFailure;
  if (status == DNSServerTester::kStatusSuccess) {
    result = Metrics::kFallbackDNSTestResultSuccess;

    // Switch to fallback DNS server if service is configured to allow DNS
    // fallback.
    CHECK(selected_service_);
    if (selected_service_->is_dns_auto_fallback_allowed()) {
      SwitchDNSServers(vector<string>(std::begin(kFallbackDnsServers),
                                      std::end(kFallbackDnsServers)));
      // Restart the portal detection with the new DNS setting.
      RestartPortalDetection();
    }
  }
  metrics()->NotifyFallbackDNSTestResult(technology_, result);
  fallback_dns_server_tester_.reset();
}

void Device::SwitchDNSServers(const vector<string> &dns_servers) {
  CHECK(ipconfig_);
  CHECK(connection_);
  // Push new DNS servers setting to the IP config object.
  ipconfig_->UpdateDNSServers(dns_servers);
  // Push new DNS servers setting to the current connection, so the resolver
  // will be updated to use the new DNS servers.
  connection_->UpdateDNSServers(dns_servers);
  // Allow the service to notify Chrome of ipconfig changes.
  selected_service_->NotifyIPConfigChanges();
}

void Device::set_traffic_monitor(TrafficMonitor *traffic_monitor) {
  traffic_monitor_.reset(traffic_monitor);
}

bool Device::IsTrafficMonitorEnabled() const {
  return false;
}

void Device::StartTrafficMonitor() {
  // Return if traffic monitor is not enabled for this device.
  if (!IsTrafficMonitorEnabled()) {
    return;
  }

  SLOG(Device, 2) << "Device " << FriendlyName()
                  << ": Traffic Monitor starting.";
  if (!traffic_monitor_.get()) {
    traffic_monitor_.reset(new TrafficMonitor(this, dispatcher_));
    traffic_monitor_->set_network_problem_detected_callback(
        Bind(&Device::OnEncounterNetworkProblem,
             weak_ptr_factory_.GetWeakPtr()));
  }
  traffic_monitor_->Start();
}

void Device::StopTrafficMonitor() {
  // Return if traffic monitor is not enabled for this device.
  if (!IsTrafficMonitorEnabled()) {
    return;
  }

  if (traffic_monitor_.get()) {
    SLOG(Device, 2) << "Device " << FriendlyName()
                    << ": Traffic Monitor stopping.";
    traffic_monitor_->Stop();
  }
  traffic_monitor_.reset();
}

void Device::OnEncounterNetworkProblem(int reason) {
  int metric_code;
  switch (reason) {
    case TrafficMonitor::kNetworkProblemCongestedTxQueue:
      metric_code = Metrics::kNetworkProblemCongestedTCPTxQueue;
      break;
    case TrafficMonitor::kNetworkProblemDNSFailure:
      metric_code = Metrics::kNetworkProblemDNSFailure;
      break;
    default:
      LOG(ERROR) << "Invalid network problem code: " << reason;
      return;
  }

  metrics()->NotifyNetworkProblemDetected(technology_, metric_code);
  // Stop the traffic monitor, only report the first network problem detected
  // on the connection for now.
  StopTrafficMonitor();
}

void Device::SetServiceConnectedState(Service::ConnectState state) {
  DCHECK(selected_service_.get());

  if (!selected_service_.get()) {
    LOG(ERROR) << FriendlyName() << ": "
               << "Portal detection completed but no selected service exists!";
    return;
  }

  if (!selected_service_->IsConnected()) {
    LOG(ERROR) << FriendlyName() << ": "
               << "Portal detection completed but selected service "
               << selected_service_->unique_name()
               << " is in non-connected state.";
    return;
  }

  if (state == Service::kStatePortal && connection_->is_default() &&
      manager_->GetPortalCheckInterval() != 0) {
    CHECK(portal_detector_.get());
    if (!portal_detector_->StartAfterDelay(
            manager_->GetPortalCheckURL(),
            manager_->GetPortalCheckInterval())) {
      LOG(ERROR) << "Device " << FriendlyName()
                 << ": Portal detection failed to restart: likely bad URL: "
                 << manager_->GetPortalCheckURL();
      SetServiceState(Service::kStateOnline);
      portal_detector_.reset();
      return;
    }
    SLOG(Device, 2) << "Device " << FriendlyName()
                    << ": Portal detection retrying.";
  } else {
    SLOG(Device, 2) << "Device " << FriendlyName()
                    << ": Portal will not retry.";
    portal_detector_.reset();
  }

  SetServiceState(state);
}

void Device::PortalDetectorCallback(const PortalDetector::Result &result) {
  if (!result.final) {
    SLOG(Device, 2) << "Device " << FriendlyName()
                    << ": Received non-final status: "
                    << PortalDetector::StatusToString(result.status);
    return;
  }

  SLOG(Device, 2) << "Device " << FriendlyName()
                  << ": Received final status: "
                  << PortalDetector::StatusToString(result.status);

  portal_attempts_to_online_ += result.num_attempts;

  int portal_status = Metrics::PortalDetectionResultToEnum(result);
  metrics()->SendEnumToUMA(
      metrics()->GetFullMetricName(Metrics::kMetricPortalResultSuffix,
                                   technology()),
      portal_status,
      Metrics::kPortalResultMax);

  if (result.status == PortalDetector::kStatusSuccess) {
    SetServiceConnectedState(Service::kStateOnline);

    metrics()->SendToUMA(
        metrics()->GetFullMetricName(
            Metrics::kMetricPortalAttemptsToOnlineSuffix, technology()),
        portal_attempts_to_online_,
        Metrics::kMetricPortalAttemptsToOnlineMin,
        Metrics::kMetricPortalAttemptsToOnlineMax,
        Metrics::kMetricPortalAttemptsToOnlineNumBuckets);
  } else {
    SetServiceConnectedState(Service::kStatePortal);

    metrics()->SendToUMA(
        metrics()->GetFullMetricName(
            Metrics::kMetricPortalAttemptsSuffix, technology()),
        result.num_attempts,
        Metrics::kMetricPortalAttemptsMin,
        Metrics::kMetricPortalAttemptsMax,
        Metrics::kMetricPortalAttemptsNumBuckets);

    // Perform fallback DNS test if the portal failure is DNS related.
    // The test will send a  DNS request to Google's DNS server to determine
    // if the DNS failure is due to bad DNS server settings.
    if ((portal_status == Metrics::kPortalResultDNSFailure) ||
        (portal_status == Metrics::kPortalResultDNSTimeout)) {
      PerformFallbackDNSTest();
    }
  }
}

vector<string> Device::AvailableIPConfigs(Error */*error*/) {
  vector<string> ipconfigs;
  if (ipconfig_) {
    ipconfigs.push_back(ipconfig_->GetRpcIdentifier());
  }
  if (ip6config_) {
    ipconfigs.push_back(ip6config_->GetRpcIdentifier());
  }
  return ipconfigs;
}

string Device::GetRpcConnectionIdentifier() {
  return adaptor_->GetRpcConnectionIdentifier();
}

uint64 Device::GetLinkMonitorResponseTime(Error *error) {
  if (!link_monitor_.get()) {
    // It is not strictly an error that the link monitor does not
    // exist, but returning an error here allows the GetProperties
    // call in our Adaptor to omit this parameter.
    error->Populate(Error::kNotFound, "Device is not running LinkMonitor");
    return 0;
  }
  return link_monitor_->GetResponseTimeMilliseconds();
}

uint64 Device::GetReceiveByteCount() {
  uint64 rx_byte_count = 0, tx_byte_count = 0;
  manager_->device_info()->GetByteCounts(
      interface_index_, &rx_byte_count, &tx_byte_count);
  return rx_byte_count - receive_byte_offset_;
}

uint64 Device::GetTransmitByteCount() {
  uint64 rx_byte_count = 0, tx_byte_count = 0;
  manager_->device_info()->GetByteCounts(
      interface_index_, &rx_byte_count, &tx_byte_count);
  return tx_byte_count - transmit_byte_offset_;
}

uint64 Device::GetReceiveByteCountProperty(Error */*error*/) {
  return GetReceiveByteCount();
}

uint64 Device::GetTransmitByteCountProperty(Error */*error*/) {
  return GetTransmitByteCount();
}

bool Device::IsUnderlyingDeviceEnabled() const {
  return false;
}

// callback
void Device::OnEnabledStateChanged(const ResultCallback &callback,
                                   const Error &error) {
  SLOG(Device, 2) << __func__
                  << " (target: " << enabled_pending_ << ","
                  << " success: " << error.IsSuccess() << ")"
                  << " on " << link_name_;
  if (error.IsSuccess()) {
    enabled_ = enabled_pending_;
    manager_->UpdateEnabledTechnologies();
    adaptor_->EmitBoolChanged(kPoweredProperty, enabled_);
  }
  enabled_pending_ = enabled_;
  if (!callback.is_null())
    callback.Run(error);
}

void Device::SetEnabled(bool enable) {
  SLOG(Device, 2) << __func__ << "(" << enable << ")";
  Error error;
  SetEnabledChecked(enable, false, &error, ResultCallback());

  // SetEnabledInternal might fail here if there is an unfinished enable or
  // disable operation. Don't log error in this case, as this method is only
  // called when the underlying device is already in the target state and the
  // pending operation should eventually bring the device to the expected
  // state.
  LOG_IF(ERROR,
         error.IsFailure() &&
         !error.IsOngoing() &&
         error.type() != Error::kInProgress)
      << "Enabled failed, but no way to report the failure.";
}

void Device::SetEnabledNonPersistent(bool enable,
                                     Error *error,
                                     const ResultCallback &callback) {
  SetEnabledChecked(enable, false, error, callback);
}

void Device::SetEnabledPersistent(bool enable,
                                  Error *error,
                                  const ResultCallback &callback) {
  SetEnabledChecked(enable, true, error, callback);
}

void Device::SetEnabledChecked(bool enable,
                               bool persist,
                               Error *error,
                               const ResultCallback &callback) {
  DCHECK(error);
  SLOG(Device, 2) << "Device " << link_name_ << " "
                  << (enable ? "starting" : "stopping");
  if (enable == enabled_) {
    if (enable != enabled_pending_ && persist) {
      // Return an error, as there is an ongoing operation to achieve the
      // opposite.
      Error::PopulateAndLog(
          error, Error::kOperationFailed,
          enable ? "Cannot enable while the device is disabling." :
                   "Cannot disable while the device is enabling.");
      return;
    }
    LOG(INFO) << "Already in desired enable state.";
    error->Reset();
    return;
  }

  if (enabled_pending_ == enable) {
    Error::PopulateAndLog(error, Error::kInProgress,
                          "Enable operation already in progress");
    return;
  }

  if (persist) {
    enabled_persistent_ = enable;
    manager_->UpdateDevice(this);
  }

  SetEnabledUnchecked(enable, error, callback);
}

void Device::SetEnabledUnchecked(bool enable, Error *error,
                                 const ResultCallback &on_enable_complete) {
  enabled_pending_ = enable;
  EnabledStateChangedCallback chained_callback =
      Bind(&Device::OnEnabledStateChanged,
           weak_ptr_factory_.GetWeakPtr(), on_enable_complete);
  if (enable) {
    running_ = true;
    Start(error, chained_callback);
  } else {
    running_ = false;
    DestroyIPConfig();         // breaks a reference cycle
    SelectService(NULL);       // breaks a reference cycle
    rtnl_handler_->SetInterfaceFlags(interface_index(), 0, IFF_UP);
    SLOG(Device, 3) << "Device " << link_name_ << " ipconfig_ "
                    << (ipconfig_ ? "is set." : "is not set.");
    SLOG(Device, 3) << "Device " << link_name_ << " ip6config_ "
                    << (ip6config_ ? "is set." : "is not set.");
    SLOG(Device, 3) << "Device " << link_name_ << " connection_ "
                    << (connection_ ? "is set." : "is not set.");
    SLOG(Device, 3) << "Device " << link_name_ << " selected_service_ "
                    << (selected_service_ ? "is set." : "is not set.");
    Stop(error, chained_callback);
  }
}

void Device::UpdateIPConfigsProperty() {
  adaptor_->EmitRpcIdentifierArrayChanged(
      kIPConfigsProperty, AvailableIPConfigs(NULL));
}

}  // namespace shill
