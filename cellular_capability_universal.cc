// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_capability_universal.h"

#include <base/bind.h>
#include <base/logging.h>
#include <base/stl_util.h>
#include <base/string_number_conversions.h>
#include <base/stringprintf.h>
#include <chromeos/dbus/service_constants.h>
#include <mobile_provider.h>
#include <mm/ModemManager-names.h>

#include <string>
#include <vector>

#include "shill/adaptor_interfaces.h"
#include "shill/cellular_service.h"
#include "shill/error.h"
#include "shill/property_accessor.h"
#include "shill/proxy_factory.h"

#ifdef MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN
#error "Do not include mm-modem.h"
#endif

// The following are constants that should be found in
// mm/ModemManager-names.h  The are reproduced here as #define because
// that is how they will appear eventually in ModemManager-names.h
#define MM_MODEM_SIMPLE_CONNECT_PIN "pin"
#define MM_MODEM_SIMPLE_CONNECT_OPERATOR_ID "operator-id"
#define MM_MODEM_SIMPLE_CONNECT_BANDS "bands"
#define MM_MODEM_SIMPLE_CONNECT_ALLWOED_MODES "allowed-modes"
#define MM_MODEM_SIMPLE_CONNECT_PREFERRED_MODE "preferred-mode"
#define MM_MODEM_SIMPLE_CONNECT_APN "apn"
#define MM_MODEM_SIMPLE_CONNECT_IP_TYPE "ip-type"
#define MM_MODEM_SIMPLE_CONNECT_USER "user"
#define MM_MODEM_SIMPLE_CONNECT_PASSWORD "password"
#define MM_MODEM_SIMPLE_CONNECT_NUMBER "number"
#define MM_MODEM_SIMPLE_CONNECT_ALLOW_ROAMING "allow-roaming"
#define MM_MODEM_SIMPLE_CONNECT_RM_PROTOCOL "rm-protocol"

using base::Bind;
using base::Callback;
using base::Closure;
using std::string;
using std::vector;

namespace shill {

// static
unsigned int CellularCapabilityUniversal::friendly_service_name_id_ = 0;

static const char kPhoneNumber[] = "*99#";

static string AccessTechnologyToString(uint32 access_technologies) {
  if (access_technologies & MM_MODEM_ACCESS_TECHNOLOGY_LTE)
    return flimflam::kNetworkTechnologyLte;
  if (access_technologies & (MM_MODEM_ACCESS_TECHNOLOGY_EVDO0 |
                              MM_MODEM_ACCESS_TECHNOLOGY_EVDOA |
                              MM_MODEM_ACCESS_TECHNOLOGY_EVDOB))
    return flimflam::kNetworkTechnologyEvdo;
  if (access_technologies & MM_MODEM_ACCESS_TECHNOLOGY_1XRTT)
    return flimflam::kNetworkTechnology1Xrtt;
  if (access_technologies & MM_MODEM_ACCESS_TECHNOLOGY_HSPA_PLUS)
    return flimflam::kNetworkTechnologyHspaPlus;
  if (access_technologies & (MM_MODEM_ACCESS_TECHNOLOGY_HSPA |
                              MM_MODEM_ACCESS_TECHNOLOGY_HSUPA |
                              MM_MODEM_ACCESS_TECHNOLOGY_HSDPA))
    return flimflam::kNetworkTechnologyHspa;
  if (access_technologies & MM_MODEM_ACCESS_TECHNOLOGY_UMTS)
    return flimflam::kNetworkTechnologyUmts;
  if (access_technologies & MM_MODEM_ACCESS_TECHNOLOGY_EDGE)
    return flimflam::kNetworkTechnologyEdge;
  if (access_technologies & MM_MODEM_ACCESS_TECHNOLOGY_GPRS)
    return flimflam::kNetworkTechnologyGprs;
  if (access_technologies & (MM_MODEM_ACCESS_TECHNOLOGY_GSM_COMPACT |
                              MM_MODEM_ACCESS_TECHNOLOGY_GSM))
      return flimflam::kNetworkTechnologyGsm;
  return "";
}

CellularCapabilityUniversal::CellularCapabilityUniversal(
    Cellular *cellular,
    ProxyFactory *proxy_factory)
    : CellularCapability(cellular, proxy_factory),
      weak_ptr_factory_(this),
      registration_state_(MM_MODEM_3GPP_REGISTRATION_STATE_UNKNOWN),
      cdma_registration_state_(MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN),
      access_technologies_(MM_MODEM_ACCESS_TECHNOLOGY_UNKNOWN),
      home_provider_(NULL),
      scanning_supported_(true),
      scanning_(false),
      scan_interval_(0) {
  VLOG(2) << "Cellular capability constructed: Universal";
  PropertyStore *store = cellular->mutable_store();

  store->RegisterConstString(flimflam::kCarrierProperty, &carrier_);
  store->RegisterConstBool(flimflam::kSupportNetworkScanProperty,
                           &scanning_supported_);
  store->RegisterConstString(flimflam::kEsnProperty, &esn_);
  store->RegisterConstString(flimflam::kFirmwareRevisionProperty,
                             &firmware_revision_);
  store->RegisterConstString(flimflam::kHardwareRevisionProperty,
                             &hardware_revision_);
  store->RegisterConstString(flimflam::kImeiProperty, &imei_);
  store->RegisterConstString(flimflam::kImsiProperty, &imsi_);
  store->RegisterConstString(flimflam::kManufacturerProperty, &manufacturer_);
  store->RegisterConstString(flimflam::kMdnProperty, &mdn_);
  store->RegisterConstString(flimflam::kMeidProperty, &meid_);
  store->RegisterConstString(flimflam::kMinProperty, &min_);
  store->RegisterConstString(flimflam::kModelIDProperty, &model_id_);
  store->RegisterConstString(flimflam::kSelectedNetworkProperty,
                             &selected_network_);
  store->RegisterConstStringmaps(flimflam::kFoundNetworksProperty,
                                 &found_networks_);
  store->RegisterConstBool(flimflam::kScanningProperty, &scanning_);
  store->RegisterUint16(flimflam::kScanIntervalProperty, &scan_interval_);
  HelpRegisterDerivedKeyValueStore(
      flimflam::kSIMLockStatusProperty,
      &CellularCapabilityUniversal::SimLockStatusToProperty,
      NULL);
  store->RegisterConstStringmaps(flimflam::kCellularApnListProperty,
                                 &apn_list_);
}

KeyValueStore CellularCapabilityUniversal::SimLockStatusToProperty(
    Error */*error*/) {
  KeyValueStore status;
  status.SetBool(flimflam::kSIMLockEnabledProperty, sim_lock_status_.enabled);
  status.SetString(flimflam::kSIMLockTypeProperty, sim_lock_status_.lock_type);
  status.SetUint(flimflam::kSIMLockRetriesLeftProperty,
                 sim_lock_status_.retries_left);
  return status;
}

void CellularCapabilityUniversal::HelpRegisterDerivedKeyValueStore(
    const string &name,
    KeyValueStore(CellularCapabilityUniversal::*get)(Error *error),
    void(CellularCapabilityUniversal::*set)(
        const KeyValueStore &value, Error *error)) {
  cellular()->mutable_store()->RegisterDerivedKeyValueStore(
      name,
      KeyValueStoreAccessor(
          new CustomAccessor<CellularCapabilityUniversal, KeyValueStore>(
              this, get, set)));
}

void CellularCapabilityUniversal::InitProxies() {
  modem_3gpp_proxy_.reset(
      proxy_factory()->CreateMM1ModemModem3gppProxy(cellular()->dbus_path(),
                                                    cellular()->dbus_owner()));
  modem_cdma_proxy_.reset(
      proxy_factory()->CreateMM1ModemModemCdmaProxy(cellular()->dbus_path(),
                                                    cellular()->dbus_owner()));
  modem_proxy_.reset(
      proxy_factory()->CreateMM1ModemProxy(cellular()->dbus_path(),
                                           cellular()->dbus_owner()));
  modem_simple_proxy_.reset(
      proxy_factory()->CreateMM1ModemSimpleProxy(cellular()->dbus_path(),
                                                 cellular()->dbus_owner()));
  // Do not create a SIM proxy until the device is enabled because we
  // do not yet know the object path of the sim object.
  // TODO(jglasgow): register callbacks
}

void CellularCapabilityUniversal::StartModem(Error *error,
                                             const ResultCallback &callback) {
  VLOG(2) << __func__;

  InitProxies();

  // Start by trying to enable the modem
  CHECK(!callback.is_null());
  modem_proxy_->Enable(
      true,
      error,
      Bind(&CellularCapabilityUniversal::Start_EnableModemCompleted,
           weak_ptr_factory_.GetWeakPtr(), callback),
      kTimeoutEnable);
  if (error->IsFailure())
    callback.Run(*error);
}

void CellularCapabilityUniversal::Start_EnableModemCompleted(
    const ResultCallback &callback, const Error &error) {
  if (error.IsFailure()) {
    callback.Run(error);
    return;
  }

  // After modem is enabled, it should be possible to get properties
  // TODO(jglasgow): handle errors from GetProperties
  GetProperties();

  // Try to register
  Error local_error;
  modem_3gpp_proxy_->Register(
      selected_network_, &local_error,
      Bind(&CellularCapabilityUniversal::Start_RegisterCompleted,
           weak_ptr_factory_.GetWeakPtr(), callback),
      kTimeoutRegister);
  if (local_error.IsFailure()) {
    callback.Run(local_error);
    return;
  }
}

void CellularCapabilityUniversal::Start_RegisterCompleted(
    const ResultCallback &callback, const Error &error) {
  if (error.IsSuccess()) {
    // If registered, get the registration state and signal quality.
    GetRegistrationState();
    GetSignalQuality();
  } else {
    LOG(ERROR) << "registration failed: " << error;
  }

  // Ignore registration errors, because that just means there is no signal.
  callback.Run(Error());
}

void CellularCapabilityUniversal::StopModem(Error *error,
                                      const ResultCallback &callback) {
  VLOG(2) << __func__;
  CHECK(!callback.is_null());
  CHECK(error);
  bool connected = false;
  string all_bearers("/");  // Represents all bearers for disconnect operations

  if (connected) {
    modem_simple_proxy_->Disconnect(
        all_bearers,
        error,
        Bind(&CellularCapabilityUniversal::Stop_DisconnectCompleted,
             weak_ptr_factory_.GetWeakPtr(), callback),
        kTimeoutDefault);
    if (error->IsFailure())
      callback.Run(*error);
  } else {
    Error error;
    Closure task = Bind(&CellularCapabilityUniversal::Stop_Disable,
                        weak_ptr_factory_.GetWeakPtr(),
                        callback);
    cellular()->dispatcher()->PostTask(task);
  }
}

void CellularCapabilityUniversal::Stop_DisconnectCompleted(
    const ResultCallback &callback, const Error &error) {
  VLOG(2) << __func__;

  LOG_IF(ERROR, error.IsFailure()) << "Disconnect failed.  Ignoring.";
  Stop_Disable(callback);
}

void CellularCapabilityUniversal::Stop_Disable(const ResultCallback &callback) {
  Error error;
  modem_proxy_->Enable(
      false, &error,
      Bind(&CellularCapabilityUniversal::Stop_DisableCompleted,
           weak_ptr_factory_.GetWeakPtr(), callback),
      kTimeoutDefault);
  if (error.IsFailure())
    callback.Run(error);
}

void CellularCapabilityUniversal::Stop_DisableCompleted(
    const ResultCallback &callback, const Error &error) {
  VLOG(2) << __func__;

  if (error.IsSuccess())
    ReleaseProxies();
  callback.Run(error);
}

void CellularCapabilityUniversal::Connect(const DBusPropertiesMap &properties,
                                          Error *error,
                                          const ResultCallback &callback) {
  OnUnsupportedOperation(__func__, error);
}

void CellularCapabilityUniversal::Disconnect(Error *error,
                                             const ResultCallback &callback) {
  OnUnsupportedOperation(__func__, error);
}

void CellularCapabilityUniversal::Activate(const string &carrier,
                                           Error *error,
                                           const ResultCallback &callback) {
  OnUnsupportedOperation(__func__, error);
}

void CellularCapabilityUniversal::ReleaseProxies() {
  VLOG(2) << __func__;
  modem_3gpp_proxy_.reset();
  modem_cdma_proxy_.reset();
  modem_proxy_.reset();
  modem_simple_proxy_.reset();
  sim_proxy_.reset();
}

void CellularCapabilityUniversal::OnServiceCreated() {
  // If IMSI is available, base the service's storage identifier on it.
  if (!imsi_.empty()) {
    cellular()->service()->SetStorageIdentifier(
        string(flimflam::kTypeCellular) + "_" +
        cellular()->address() + "_" + imsi_);
  }
  cellular()->service()->SetActivationState(
      flimflam::kActivationStateActivated);
  UpdateServingOperator();
}

void CellularCapabilityUniversal::UpdateStatus(
    const DBusPropertiesMap &properties) {
  if (ContainsKey(properties, kPropertyIMSI)) {
    SetHomeProvider();
  }
}

// Create the list of APNs to try, in the following order:
// - last APN that resulted in a successful connection attempt on the
//   current network (if any)
// - the APN, if any, that was set by the user
// - the list of APNs found in the mobile broadband provider DB for the
//   home provider associated with the current SIM
// - as a last resort, attempt to connect with no APN
void CellularCapabilityUniversal::SetupApnTryList() {
  apn_try_list_.clear();

  DCHECK(cellular()->service().get());
  const Stringmap *apn_info = cellular()->service()->GetLastGoodApn();
  if (apn_info)
    apn_try_list_.push_back(*apn_info);

  apn_info = cellular()->service()->GetUserSpecifiedApn();
  if (apn_info)
    apn_try_list_.push_back(*apn_info);

  apn_try_list_.insert(apn_try_list_.end(), apn_list_.begin(), apn_list_.end());
}

void CellularCapabilityUniversal::SetupConnectProperties(
    DBusPropertiesMap *properties) {
  SetupApnTryList();
  FillConnectPropertyMap(properties);
}

void CellularCapabilityUniversal::FillConnectPropertyMap(
    DBusPropertiesMap *properties) {

  // TODO(jglasgow): Is this really needed anymore?
  (*properties)[MM_MODEM_SIMPLE_CONNECT_NUMBER].writer().append_string(
      kPhoneNumber);

  (*properties)[MM_MODEM_SIMPLE_CONNECT_ALLOW_ROAMING].writer().append_bool(
      AllowRoaming());

  if (!apn_try_list_.empty()) {
    // Leave the APN at the front of the list, so that it can be recorded
    // if the connect attempt succeeds.
    Stringmap apn_info = apn_try_list_.front();
    VLOG(2) << __func__ << ": Using APN " << apn_info[flimflam::kApnProperty];
    (*properties)[MM_MODEM_SIMPLE_CONNECT_APN].writer().append_string(
        apn_info[flimflam::kApnProperty].c_str());
    if (ContainsKey(apn_info, flimflam::kApnUsernameProperty))
      (*properties)[MM_MODEM_SIMPLE_CONNECT_USER].writer().append_string(
          apn_info[flimflam::kApnUsernameProperty].c_str());
    if (ContainsKey(apn_info, flimflam::kApnPasswordProperty))
      (*properties)[MM_MODEM_SIMPLE_CONNECT_PASSWORD].writer().append_string(
          apn_info[flimflam::kApnPasswordProperty].c_str());
  }
}

void CellularCapabilityUniversal::OnConnectReply(const ResultCallback &callback,
                                           const Error &error) {
  if (error.IsFailure()) {
    cellular()->service()->ClearLastGoodApn();
    // The APN that was just tried (and failed) is still at the
    // front of the list, about to be removed. If the list is empty
    // after that, try one last time without an APN. This may succeed
    // with some modems in some cases.
    if (error.type() == Error::kInvalidApn && !apn_try_list_.empty()) {
      apn_try_list_.pop_front();
      VLOG(2) << "Connect failed with invalid APN, " << apn_try_list_.size()
              << " remaining APNs to try";
      DBusPropertiesMap props;
      FillConnectPropertyMap(&props);
      Error error;
      Connect(props, &error, callback);
      return;
    }
    cellular()->OnConnectFailed(error);
  } else {
    if (!apn_try_list_.empty()) {
      cellular()->service()->SetLastGoodApn(apn_try_list_.front());
      apn_try_list_.clear();
    }
    cellular()->OnConnected();
  }

  if (!callback.is_null())
    callback.Run(error);
}

bool CellularCapabilityUniversal::AllowRoaming() {
  bool requires_roaming =
      home_provider_ ? home_provider_->requires_roaming : false;
  return requires_roaming || allow_roaming_property();
}

void CellularCapabilityUniversal::GetRegistrationState() {
  VLOG(2) << __func__;
  string operator_code;
  string operator_name;

  const MMModem3gppRegistrationState state =
      static_cast<MMModem3gppRegistrationState>(
          modem_3gpp_proxy_->RegistrationState());
  if (state == MM_MODEM_3GPP_REGISTRATION_STATE_HOME ||
      state == MM_MODEM_3GPP_REGISTRATION_STATE_ROAMING) {
    operator_code = modem_3gpp_proxy_->OperatorCode();
    operator_name = modem_3gpp_proxy_->OperatorName();
  }
  On3GPPRegistrationChanged(state, operator_code, operator_name);
}

void CellularCapabilityUniversal::GetProperties() {
  VLOG(2) << __func__;

  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  uint32 technologies = modem_proxy_->AccessTechnologies();
  // TODO(jglasgow): figure out the most likely one that we are using....
  SetAccessTechnologies(technologies);
  VLOG(2) << "AccessTechnologies: " << technologies;

  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  uint32 locks = modem_3gpp_proxy_->EnabledFacilityLocks();
  sim_lock_status_.enabled = locks & MM_MODEM_3GPP_FACILITY_SIM;
  VLOG(2) << "GSM EnabledFacilityLocks: " << locks;

  // TODO(jglasgow): Switch to asynchronous calls (crosbug.com/17583).
  const DBus::Struct<unsigned int, bool> quality =
      modem_proxy_->SignalQuality();
  OnSignalQualityChanged(quality._1);

  // TODO(jglasgow): Switch to asynchronous calls (crosbug.com/17583).
  if (imei_.empty()) {
    imei_ = modem_3gpp_proxy_->Imei();
  }

  string sim_path = modem_proxy_->Sim();
  OnSimPathChanged(sim_path);

  if (sim_proxy_.get()) {
    if (imsi_.empty()) {
      imsi_ = sim_proxy_->Imsi();
    }
    if (spn_.empty()) {
      spn_ = sim_proxy_->OperatorName();
      // TODO(jglasgow): May eventually want to get SPDI, etc
    }
  }

  if (mdn_.empty()) {
    // TODO(njw): Switch to asynchronous calls (crosbug.com/17583).
    vector<string> numbers = modem_proxy_->OwnNumbers();
    if (numbers.size() > 0)
      mdn_ = numbers[0];
  }

  if (model_id_.empty()) {
    // TODO(njw): Switch to asynchronous calls (crosbug.com/17583).
    model_id_ = modem_proxy_->Model();
  }

  if (manufacturer_.empty()) {
    // TODO(njw): Switch to asynchronous calls (crosbug.com/17583).
    manufacturer_ = modem_proxy_->Manufacturer();
  }

  if (firmware_revision_.empty()) {
    // TODO(njw): Switch to asynchronous calls (crosbug.com/17583).
    firmware_revision_ = modem_proxy_->Revision();
  }

  GetRegistrationState();
}

string CellularCapabilityUniversal::CreateFriendlyServiceName() {
  VLOG(2) << __func__;
  if (registration_state_ == MM_MODEM_3GPP_REGISTRATION_STATE_HOME &&
      !cellular()->home_provider().GetName().empty()) {
    return cellular()->home_provider().GetName();
  }
  if (!serving_operator_.GetName().empty()) {
    return serving_operator_.GetName();
  }
  if (!carrier_.empty()) {
    return carrier_;
  }
  if (!serving_operator_.GetCode().empty()) {
    return "cellular_" + serving_operator_.GetCode();
  }
  return base::StringPrintf("GSMNetwork%u", friendly_service_name_id_++);
}

void CellularCapabilityUniversal::SetHomeProvider() {
  VLOG(2) << __func__ << "(IMSI: " << imsi_
          << " SPN: " << spn_ << ")";
  // TODO(petkov): The test for NULL provider_db should be done by
  // mobile_provider_lookup_best_match.
  if (imsi_.empty() || !cellular()->provider_db()) {
    return;
  }
  mobile_provider *provider =
      mobile_provider_lookup_best_match(
          cellular()->provider_db(), spn_.c_str(), imsi_.c_str());
  if (!provider) {
    VLOG(2) << "GSM provider not found.";
    return;
  }
  home_provider_ = provider;
  Cellular::Operator oper;
  if (provider->networks) {
    oper.SetCode(provider->networks[0]);
  }
  if (provider->country) {
    oper.SetCountry(provider->country);
  }
  if (spn_.empty()) {
    const char *name = mobile_provider_get_name(provider);
    if (name) {
      oper.SetName(name);
    }
  } else {
    oper.SetName(spn_);
  }
  cellular()->set_home_provider(oper);
  InitAPNList();
}

void CellularCapabilityUniversal::UpdateOperatorInfo() {
  VLOG(2) << __func__;
  const string &network_id = serving_operator_.GetCode();
  if (!network_id.empty()) {
    VLOG(2) << "Looking up network id: " << network_id;
    mobile_provider *provider =
        mobile_provider_lookup_by_network(cellular()->provider_db(),
                                          network_id.c_str());
    if (provider) {
      const char *provider_name = mobile_provider_get_name(provider);
      if (provider_name && *provider_name) {
        serving_operator_.SetName(provider_name);
        if (provider->country) {
          serving_operator_.SetCountry(provider->country);
        }
        VLOG(2) << "Operator name: " << serving_operator_.GetName()
                << ", country: " << serving_operator_.GetCountry();
      }
    } else {
      VLOG(2) << "GSM provider not found.";
    }
  }
  UpdateServingOperator();
}

void CellularCapabilityUniversal::UpdateServingOperator() {
  VLOG(2) << __func__;
  if (cellular()->service().get()) {
    cellular()->service()->SetServingOperator(serving_operator_);
  }
}

void CellularCapabilityUniversal::InitAPNList() {
  VLOG(2) << __func__;
  if (!home_provider_) {
    return;
  }
  apn_list_.clear();
  for (int i = 0; i < home_provider_->num_apns; ++i) {
    Stringmap props;
    mobile_apn *apn = home_provider_->apns[i];
    if (apn->value) {
      props[flimflam::kApnProperty] = apn->value;
    }
    if (apn->username) {
      props[flimflam::kApnUsernameProperty] = apn->username;
    }
    if (apn->password) {
      props[flimflam::kApnPasswordProperty] = apn->password;
    }
    // Find the first localized and non-localized name, if any.
    const localized_name *lname = NULL;
    const localized_name *name = NULL;
    for (int j = 0; j < apn->num_names; ++j) {
      if (apn->names[j]->lang) {
        if (!lname) {
          lname = apn->names[j];
        }
      } else if (!name) {
        name = apn->names[j];
      }
    }
    if (name) {
      props[flimflam::kApnNameProperty] = name->name;
    }
    if (lname) {
      props[flimflam::kApnLocalizedNameProperty] = lname->name;
      props[flimflam::kApnLanguageProperty] = lname->lang;
    }
    apn_list_.push_back(props);
  }
  cellular()->adaptor()->EmitStringmapsChanged(
      flimflam::kCellularApnListProperty, apn_list_);
}

// always called from an async context
void CellularCapabilityUniversal::Register(const ResultCallback &callback) {
  VLOG(2) << __func__ << " \"" << selected_network_ << "\"";
  CHECK(!callback.is_null());
  Error error;
  ResultCallback cb = Bind(&CellularCapabilityUniversal::OnRegisterReply,
                                weak_ptr_factory_.GetWeakPtr(), callback);
  modem_3gpp_proxy_->Register(selected_network_, &error, cb, kTimeoutRegister);
  if (error.IsFailure())
    callback.Run(error);
}

void CellularCapabilityUniversal::RegisterOnNetwork(
    const string &network_id,
    Error *error,
    const ResultCallback &callback) {
  VLOG(2) << __func__ << "(" << network_id << ")";
  CHECK(error);
  desired_network_ = network_id;
  ResultCallback cb = Bind(&CellularCapabilityUniversal::OnRegisterReply,
                                weak_ptr_factory_.GetWeakPtr(), callback);
  modem_3gpp_proxy_->Register(network_id, error, cb, kTimeoutRegister);
}

void CellularCapabilityUniversal::OnRegisterReply(
    const ResultCallback &callback,
    const Error &error) {
  VLOG(2) << __func__ << "(" << error << ")";

  if (error.IsSuccess()) {
    selected_network_ = desired_network_;
    desired_network_.clear();
    callback.Run(error);
    return;
  }
  // If registration on the desired network failed,
  // try to register on the home network.
  if (!desired_network_.empty()) {
    desired_network_.clear();
    selected_network_.clear();
    LOG(INFO) << "Couldn't register on selected network, trying home network";
    Register(callback);
    return;
  }
  callback.Run(error);
}

bool CellularCapabilityUniversal::IsRegistered() {
  return (registration_state_ == MM_MODEM_3GPP_REGISTRATION_STATE_HOME ||
          registration_state_ == MM_MODEM_3GPP_REGISTRATION_STATE_ROAMING);
}

void CellularCapabilityUniversal::RequirePIN(
    const string &pin, bool require,
    Error *error, const ResultCallback &callback) {
  CHECK(error);
  sim_proxy_->EnablePin(pin, require, error, callback, kTimeoutDefault);
}

void CellularCapabilityUniversal::EnterPIN(const string &pin,
                                           Error *error,
                                           const ResultCallback &callback) {
  CHECK(error);
  sim_proxy_->SendPin(pin, error, callback, kTimeoutDefault);
}

void CellularCapabilityUniversal::UnblockPIN(const string &unblock_code,
                                             const string &pin,
                                             Error *error,
                                             const ResultCallback &callback) {
  CHECK(error);
  sim_proxy_->SendPuk(unblock_code, pin, error, callback, kTimeoutDefault);
}

void CellularCapabilityUniversal::ChangePIN(
    const string &old_pin, const string &new_pin,
    Error *error, const ResultCallback &callback) {
  CHECK(error);
  sim_proxy_->ChangePin(old_pin, new_pin, error, callback, kTimeoutDefault);
}

void CellularCapabilityUniversal::Scan(Error *error,
                                       const ResultCallback &callback) {
  VLOG(2) << __func__;
  // TODO(petkov): Defer scan requests if a scan is in progress already.
  CHECK(error);
  DBusPropertyMapsCallback cb = Bind(&CellularCapabilityUniversal::OnScanReply,
                                     weak_ptr_factory_.GetWeakPtr(), callback);
  modem_3gpp_proxy_->Scan(error, cb, kTimeoutScan);
}

void CellularCapabilityUniversal::OnScanReply(const ResultCallback &callback,
                                              const ScanResults &results,
                                              const Error &error) {
  VLOG(2) << __func__;

  // Error handling is weak.  The current expectation is that on any
  // error, found_networks_ should be cleared and a property change
  // notification sent out.
  //
  // TODO(jglasgow): fix error handling
  found_networks_.clear();
  if (!error.IsFailure()) {
    for (ScanResults::const_iterator it = results.begin();
         it != results.end(); ++it) {
      found_networks_.push_back(ParseScanResult(*it));
    }
  }
  cellular()->adaptor()->EmitStringmapsChanged(flimflam::kFoundNetworksProperty,
                                               found_networks_);
  callback.Run(error);
}

Stringmap CellularCapabilityUniversal::ParseScanResult(
    const ScanResult &result) {

  static const char kStatusProperty[] = "status";
  static const char kOperatorLongProperty[] = "operator-long";
  static const char kOperatorShortProperty[] = "operator-short";
  static const char kOperatorCodeProperty[] = "operator-code";
  static const char kOperatorAccessTechnologyProperty[] = "access-technology";

  /* ScanResults contain the following keys:

     "status"
     A MMModem3gppNetworkAvailability value representing network
     availability status, given as an unsigned integer (signature "u").
     This key will always be present.

     "operator-long"
     Long-format name of operator, given as a string value (signature
     "s"). If the name is unknown, this field should not be present.

     "operator-short"
     Short-format name of operator, given as a string value
     (signature "s"). If the name is unknown, this field should not
     be present.

     "operator-code"
     Mobile code of the operator, given as a string value (signature
     "s"). Returned in the format "MCCMNC", where MCC is the
     three-digit ITU E.212 Mobile Country Code and MNC is the two- or
     three-digit GSM Mobile Network Code. e.g. "31026" or "310260".

     "access-technology"
     A MMModemAccessTechnology value representing the generic access
     technology used by this mobile network, given as an unsigned
     integer (signature "u").
  */
  Stringmap parsed;

  uint32 status;
  if (DBusProperties::GetUint32(result, kStatusProperty, &status)) {
    // numerical values are taken from 3GPP TS 27.007 Section 7.3.
    static const char * const kStatusString[] = {
      "unknown",    // MM_MODEM_3GPP_NETWORK_AVAILABILITY_UNKNOWN
      "available",  // MM_MODEM_3GPP_NETWORK_AVAILABILITY_AVAILABLE
      "current",    // MM_MODEM_3GPP_NETWORK_AVAILABILITY_CURRENT
      "forbidden",  // MM_MODEM_3GPP_NETWORK_AVAILABILITY_FORBIDDEN
    };
    parsed[flimflam::kStatusProperty] = kStatusString[status];
  }

  uint32 tech;  // MMModemAccessTechnology
  if (DBusProperties::GetUint32(result, kOperatorAccessTechnologyProperty,
                                &tech)) {
    parsed[flimflam::kTechnologyProperty] = AccessTechnologyToString(tech);
  }

  string operator_long, operator_short, operator_code;
  if (DBusProperties::GetString(result, kOperatorLongProperty, &operator_long))
    parsed[flimflam::kLongNameProperty] = operator_long;
  if (DBusProperties::GetString(result, kOperatorShortProperty,
                                &operator_short))
    parsed[flimflam::kShortNameProperty] = operator_short;
  if (DBusProperties::GetString(result, kOperatorCodeProperty, &operator_code))
    parsed[flimflam::kNetworkIdProperty] = operator_code;

  // If the long name is not available but the network ID is, look up the long
  // name in the mobile provider database.
  if ((!ContainsKey(parsed, flimflam::kLongNameProperty) ||
       parsed[flimflam::kLongNameProperty].empty()) &&
      ContainsKey(parsed, flimflam::kNetworkIdProperty)) {
    mobile_provider *provider =
        mobile_provider_lookup_by_network(
            cellular()->provider_db(),
            parsed[flimflam::kNetworkIdProperty].c_str());
    if (provider) {
      const char *long_name = mobile_provider_get_name(provider);
      if (long_name && *long_name) {
        parsed[flimflam::kLongNameProperty] = long_name;
      }
    }
  }
  return parsed;
}

void CellularCapabilityUniversal::SetAccessTechnologies(
    uint32 access_technologies) {
  access_technologies_ = access_technologies;
  if (cellular()->service().get()) {
    cellular()->service()->SetNetworkTechnology(GetNetworkTechnologyString());
  }
}

string CellularCapabilityUniversal::GetNetworkTechnologyString() const {
  // Order is imnportant.  Return the highest speed technology
  // TODO(jglasgow): change shill interfaces to a capability model

  return AccessTechnologyToString(access_technologies_);
}

string CellularCapabilityUniversal::GetRoamingStateString() const {
  switch (registration_state_) {
    case MM_MODEM_3GPP_REGISTRATION_STATE_HOME:
      return flimflam::kRoamingStateHome;
    case MM_MODEM_3GPP_REGISTRATION_STATE_ROAMING:
      return flimflam::kRoamingStateRoaming;
    default:
      break;
  }
  return flimflam::kRoamingStateUnknown;
}

void CellularCapabilityUniversal::GetSignalQuality() {
  // TODO(jglasgow): implement
  NOTIMPLEMENTED();
}

void CellularCapabilityUniversal::OnModemPropertiesChanged(
    const DBusPropertiesMap &properties,
    const vector<string> &/* invalidated_properties */) {
  string value;
  if (DBusProperties::GetString(properties,
                                MM_MODEM_PROPERTY_SIM, &value))
    OnSimPathChanged(value);

  uint32 access_technologies = MM_MODEM_ACCESS_TECHNOLOGY_UNKNOWN;
  if (DBusProperties::GetUint32(properties,
                                MM_MODEM_PROPERTY_ACCESSTECHNOLOGIES,
                                &access_technologies)) {
    SetAccessTechnologies(access_technologies);
  }
  // Unlockrequired and SimLock
  bool emit = false;

  uint32_t lock_required;  // This is really of type MMModemLock
  if (DBusProperties::GetUint32(properties,
                                MM_MODEM_PROPERTY_UNLOCKREQUIRED,
                                &lock_required)) {
    // TODO(jglasgow): set sim_lock_status_.lock_type
    emit = true;
  }
  // TODO(jglasgow): Update PIN retries which are a{uu} and require parsing
  // Get the property MM_MODEM_PROPERTY_UNLOCKRETRIES
  // Set sim_lock_status_.retries_left

  if (emit) {
    cellular()->adaptor()->EmitKeyValueStoreChanged(
        flimflam::kSIMLockStatusProperty, SimLockStatusToProperty(NULL));
  }
}

void CellularCapabilityUniversal::OnDBusPropertiesChanged(
    const string &interface,
    const DBusPropertiesMap &changed_properties,
    const vector<string> &invalidated_properties) {
  if (interface == MM_DBUS_INTERFACE_MODEM) {
    OnModemPropertiesChanged(changed_properties, invalidated_properties);
  }
  // TODO(jglasgow): handle additional interfaces
}

void CellularCapabilityUniversal::OnModem3GPPPropertiesChanged(
    const DBusPropertiesMap &properties) {
  bool emit = false;
  uint32 locks = 0;
  if (DBusProperties::GetUint32(
          properties, MM_MODEM_MODEM3GPP_PROPERTY_ENABLEDFACILITYLOCKS,
          &locks)) {
    sim_lock_status_.enabled = locks & MM_MODEM_3GPP_FACILITY_SIM;
    emit = true;
  }
  // TODO(jglasgow): coordinate with changes to Modem properties
  if (emit) {
    cellular()->adaptor()->EmitKeyValueStoreChanged(
        flimflam::kSIMLockStatusProperty, SimLockStatusToProperty(NULL));
  }
}

void CellularCapabilityUniversal::OnNetworkModeSignal(uint32 /*mode*/) {
  // TODO(petkov): Implement this.
  NOTIMPLEMENTED();
}

void CellularCapabilityUniversal::On3GPPRegistrationChanged(
    MMModem3gppRegistrationState state,
    const string &operator_code,
    const string &operator_name) {
  VLOG(2) << __func__ << ": regstate=" << state
          << ", opercode=" << operator_code
          << ", opername=" << operator_name;
  registration_state_ = state;
  serving_operator_.SetCode(operator_code);
  serving_operator_.SetName(operator_name);
  UpdateOperatorInfo();
  cellular()->HandleNewRegistrationState();
}

void CellularCapabilityUniversal::OnSignalQualityChanged(uint32 quality) {
  cellular()->HandleNewSignalQuality(quality);
}

void CellularCapabilityUniversal::OnSimPathChanged(
    const string &sim_path) {
  if (sim_path == sim_path_)
    return;

  mm1::SimProxyInterface *proxy = NULL;
  if (!sim_path.empty())
    proxy = proxy_factory()->CreateSimProxy(sim_path,
                                            cellular()->dbus_owner());
  sim_path_ = sim_path;
  sim_proxy_.reset(proxy);
}

}  // namespace shill
