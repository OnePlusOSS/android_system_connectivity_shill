// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_H_
#define SHILL_CELLULAR_H_

#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/memory/weak_ptr.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/dbus_properties.h"
#include "shill/device.h"
#include "shill/event_dispatcher.h"
#include "shill/metrics.h"
#include "shill/modem_info.h"
#include "shill/modem_proxy_interface.h"
#include "shill/refptr_types.h"
#include "shill/rpc_task.h"

struct mobile_provider_db;

namespace shill {

class CellularCapability;
class Error;
class ExternalTask;
class PPPDeviceFactory;
class ProxyFactory;

class Cellular : public Device, public RPCTaskDelegate {
 public:
  enum Type {
    kTypeGSM,
    kTypeCDMA,
    kTypeUniversal,  // ModemManager1
    kTypeUniversalCDMA,
    kTypeInvalid,
  };

  // The device states progress linearly from Disabled to Linked.
  enum State {
    // This is the initial state of the modem and indicates that the modem radio
    // is not turned on.
    kStateDisabled,
    // This state indicates that the modem radio is turned on, and it should be
    // possible to measure signal strength.
    kStateEnabled,
    // The modem has registered with a network and has signal quality
    // measurements. A cellular service object is created.
    kStateRegistered,
    // The modem has connected to a network.
    kStateConnected,
    // The network interface is UP.
    kStateLinked,
  };

  // This enum must be kept in sync with ModemManager's MMModemState enum.
  enum ModemState {
    kModemStateFailed = -1,
    kModemStateUnknown = 0,
    kModemStateInitializing = 1,
    kModemStateLocked = 2,
    kModemStateDisabled = 3,
    kModemStateDisabling = 4,
    kModemStateEnabling = 5,
    kModemStateEnabled = 6,
    kModemStateSearching = 7,
    kModemStateRegistered = 8,
    kModemStateDisconnecting = 9,
    kModemStateConnecting = 10,
    kModemStateConnected = 11,
  };

  class Operator {
   public:
    Operator();
    ~Operator();

    void CopyFrom(const Operator &oper);
    bool Equals(const Operator &oper) const { return dict_ == oper.dict_; }

    const std::string &GetName() const;
    void SetName(const std::string &name);

    const std::string &GetCode() const;
    void SetCode(const std::string &code);

    const std::string &GetCountry() const;
    void SetCountry(const std::string &country);

    const Stringmap &ToDict() const;

   private:
    Stringmap dict_;

    DISALLOW_COPY_AND_ASSIGN(Operator);
  };

  // |owner| is the ModemManager DBus service owner (e.g., ":1.17").
  // |path| is the ModemManager.Modem DBus object path (e.g.,
  // "/org/chromium/ModemManager/Gobi/0").
  // |service| is the modem mananager service name (e.g.,
  // /org/chromium/ModemManager or /org/freedesktop/ModemManager1).
  Cellular(ModemInfo *modem_info,
           const std::string &link_name,
           const std::string &address,
           int interface_index,
           Type type,
           const std::string &owner,
           const std::string &service,
           const std::string &path,
           ProxyFactory *proxy_factory);
  virtual ~Cellular();

  // Load configuration for the device from |storage|.
  virtual bool Load(StoreInterface *storage);

  // Save configuration for the device to |storage|.
  virtual bool Save(StoreInterface *storage);

  // Asynchronously connects the modem to the network. Populates |error| on
  // failure, leaves it unchanged otherwise.
  virtual void Connect(Error *error);

  // Asynchronously disconnects the modem from the network and populates
  // |error| on failure, leaves it unchanged otherwise.
  virtual void Disconnect(Error *error);

  // Asynchronously activates the modem. Returns an error on failure.
  void Activate(const std::string &carrier, Error *error,
                const ResultCallback &callback);

  // Performs the necessary steps to bring the service to the activated state,
  // once an online payment has been done.
  void CompleteActivation(Error *error);

  const CellularServiceRefPtr &service() const { return service_; }

  // Deregisters and destructs the current service and destroys the connection,
  // if any. This also eliminates the circular references between this device
  // and the associated service, allowing eventual device destruction.
  virtual void DestroyService();

  static std::string GetStateString(State state);
  static std::string GetModemStateString(ModemState modem_state);

  std::string CreateFriendlyServiceName();

  State state() const { return state_; }

  void set_modem_state(ModemState state) { modem_state_ = state; }
  ModemState modem_state() const { return modem_state_; }
  bool IsUnderlyingDeviceEnabled() const;
  bool IsModemRegistered() const;
  static bool IsEnabledModemState(ModemState state);

  void HandleNewSignalQuality(uint32 strength);

  // Processes a change in the modem registration state, possibly creating,
  // destroying or updating the CellularService.
  void HandleNewRegistrationState();

  virtual void OnDBusPropertiesChanged(
      const std::string &interface,
      const DBusPropertiesMap &changed_properties,
      const std::vector<std::string> &invalidated_properties);

  // Inherited from Device.
  virtual void Start(Error *error, const EnabledStateChangedCallback &callback)
      override;
  virtual void Stop(Error *error, const EnabledStateChangedCallback &callback)
      override;
  virtual void LinkEvent(unsigned int flags, unsigned int change) override;
  virtual void Scan(ScanType /*scan_type*/, Error *error,
                    const std::string &/*reason*/) override;
  virtual void RegisterOnNetwork(const std::string &network_id,
                                 Error *error,
                                 const ResultCallback &callback) override;
  virtual void RequirePIN(const std::string &pin, bool require,
                          Error *error, const ResultCallback &callback)
      override;
  virtual void EnterPIN(const std::string &pin,
                        Error *error, const ResultCallback &callback) override;
  virtual void UnblockPIN(const std::string &unblock_code,
                          const std::string &pin,
                          Error *error, const ResultCallback &callback)
      override;
  virtual void ChangePIN(const std::string &old_pin,
                         const std::string &new_pin,
                         Error *error, const ResultCallback &callback) override;
  virtual void Reset(Error *error, const ResultCallback &callback) override;
  virtual void SetCarrier(const std::string &carrier,
                          Error *error, const ResultCallback &callback)
      override;
  virtual bool IsIPv6Allowed() const override;
  virtual void DropConnection() override;
  virtual void SetServiceState(Service::ConnectState state) override;
  virtual void SetServiceFailure(Service::ConnectFailure failure_state)
      override;
  virtual void SetServiceFailureSilent(Service::ConnectFailure failure_state)
      override;
  virtual void OnAfterResume() override;

  void StartModemCallback(const EnabledStateChangedCallback &callback,
                          const Error &error);
  void StopModemCallback(const EnabledStateChangedCallback &callback,
                         const Error &error);
  void OnDisabled();
  void OnEnabled();
  void OnConnecting();
  void OnConnected();
  void OnConnectFailed(const Error &error);
  void OnDisconnected();
  void OnDisconnectFailed();
  std::string GetTechnologyFamily(Error *error);
  void OnModemStateChanged(ModemState new_state);
  void OnScanReply(const Stringmaps &found_networks, const Error &error);

  // accessor to read the allow roaming property
  bool allow_roaming_property() const { return allow_roaming_; }
  // Is the underlying device in the process of activating?
  bool IsActivating() const;

  // Initiate PPP link. Called from capabilities.
  virtual void StartPPP(const std::string &serial_device);
  // Callback for |ppp_task_|.
  virtual void OnPPPDied(pid_t pid, int exit);
  // Implements RPCTaskDelegate, for |ppp_task_|.
  virtual void GetLogin(std::string *user, std::string *password) override;
  virtual void Notify(const std::string &reason,
                      const std::map<std::string, std::string> &dict) override;

  // ///////////////////////////////////////////////////////////////////////////
  // DBus Properties exposed by the Device interface of shill.
  void RegisterProperties();

  // getters
  const std::string &dbus_owner() const { return dbus_owner_; }
  const std::string &dbus_path() const { return dbus_path_; }
  const Operator &home_provider() const { return home_provider_; }
  const std::string &carrier() const { return carrier_; }
  bool scanning_supported() const { return scanning_supported_; }
  const std::string &esn() const { return esn_; }
  const std::string &firmware_revision() const { return firmware_revision_; }
  const std::string &hardware_revision() const { return hardware_revision_; }
  const std::string &imei() const { return imei_; }
  const std::string &imsi() const { return imsi_; }
  const std::string &mdn() const { return mdn_; }
  const std::string &meid() const { return meid_; }
  const std::string &min() const { return min_; }
  const std::string &manufacturer() const { return manufacturer_; }
  const std::string &model_id() const { return model_id_; }
  bool scanning() const { return scanning_; }

  const std::string &selected_network() const { return selected_network_; }
  const Stringmaps &found_networks() const { return found_networks_; }
  bool provider_requires_roaming() const { return provider_requires_roaming_; }
  bool sim_present() const { return sim_present_; }
  const Stringmaps &apn_list() const { return apn_list_; }

  // setters
  void set_home_provider(const Operator &oper);
  void set_carrier(const std::string &carrier);
  void set_scanning_supported(bool scanning_supported);
  void set_esn(const std::string &esn);
  void set_firmware_revision(const std::string &firmware_revision);
  void set_hardware_revision(const std::string &hardware_revision);
  void set_imei(const std::string &imei);
  void set_imsi(const std::string &imsi);
  void set_mdn(const std::string &mdn);
  void set_meid(const std::string &meid);
  void set_min(const std::string &min);
  void set_manufacturer(const std::string &manufacturer);
  void set_model_id(const std::string &model_id);
  void set_scanning(bool scanning);

  void set_selected_network(const std::string &selected_network);
  void clear_found_networks();
  void set_found_networks(const Stringmaps &found_networks);
  void set_provider_requires_roaming(bool provider_requires_roaming);
  void set_sim_present(bool sim_present);
  void set_apn_list(const Stringmaps &apn_list);


 private:
  friend class ActivePassiveOutOfCreditsDetectorTest;
  friend class CellularTest;
  friend class CellularCapabilityTest;
  friend class CellularCapabilityCDMATest;
  friend class CellularCapabilityGSMTest;
  friend class CellularCapabilityUniversalTest;
  friend class CellularCapabilityUniversalCDMATest;
  friend class CellularServiceTest;
  friend class ModemTest;
  friend class SubscriptionStateOutOfCreditsDetectorTest;
  FRIEND_TEST(CellularCapabilityCDMATest, CreateFriendlyServiceName);
  FRIEND_TEST(CellularCapabilityCDMATest, GetRegistrationState);
  FRIEND_TEST(CellularCapabilityGSMTest, AllowRoaming);
  FRIEND_TEST(CellularCapabilityGSMTest, CreateFriendlyServiceName);
  FRIEND_TEST(CellularCapabilityTest, AllowRoaming);
  FRIEND_TEST(CellularCapabilityTest, EnableModemFail);
  FRIEND_TEST(CellularCapabilityTest, EnableModemSucceed);
  FRIEND_TEST(CellularCapabilityTest, FinishEnable);
  FRIEND_TEST(CellularCapabilityTest, GetModemInfo);
  FRIEND_TEST(CellularCapabilityTest, GetModemStatus);
  FRIEND_TEST(CellularCapabilityUniversalCDMATest, CreateFriendlyServiceName);
  FRIEND_TEST(CellularCapabilityUniversalCDMATest, OnCDMARegistrationChanged);
  FRIEND_TEST(CellularCapabilityUniversalCDMATest, UpdateOLP);
  FRIEND_TEST(CellularCapabilityUniversalCDMATest, UpdateOperatorInfo);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, AllowRoaming);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, CreateFriendlyServiceName);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, Connect);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, IsServiceActivationRequired);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, SetHomeProvider);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, StartModemAlreadyEnabled);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, StopModemConnected);
  FRIEND_TEST(CellularCapabilityUniversalMainTest,
              UpdatePendingActivationState);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, UpdateOLP);
  FRIEND_TEST(CellularCapabilityUniversalMainTest,
              UpdateOperatorInfoViaOperatorId);
  FRIEND_TEST(CellularCapabilityUniversalMainTest,
              UpdateRegistrationState);
  FRIEND_TEST(CellularCapabilityUniversalMainTest,
              UpdateRegistrationStateModemNotConnected);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, UpdateScanningProperty);
  FRIEND_TEST(CellularCapabilityUniversalMainTest,
              UpdateServiceActivationState);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, UpdateServiceName);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, UpdateStorageIdentifier);
  FRIEND_TEST(CellularServiceTest, FriendlyName);
  FRIEND_TEST(CellularTest, ChangeServiceState);
  FRIEND_TEST(CellularTest, ChangeServiceStatePPP);
  FRIEND_TEST(CellularTest, CreateService);
  FRIEND_TEST(CellularTest, Connect);
  FRIEND_TEST(CellularTest, ConnectFailure);
  FRIEND_TEST(CellularTest, ConnectFailureNoService);
  FRIEND_TEST(CellularTest, ConnectSuccessNoService);
  FRIEND_TEST(CellularTest, CustomSetterNoopChange);
  FRIEND_TEST(CellularTest, DisableModem);
  FRIEND_TEST(CellularTest, Disconnect);
  FRIEND_TEST(CellularTest, DisconnectFailure);
  FRIEND_TEST(CellularTest, DisconnectWithCallback);
  FRIEND_TEST(CellularTest, DropConnection);
  FRIEND_TEST(CellularTest, DropConnectionPPP);
  FRIEND_TEST(CellularTest, EnableTrafficMonitor);
  FRIEND_TEST(CellularTest,
              HandleNewRegistrationStateForServiceRequiringActivation);
  FRIEND_TEST(CellularTest, LinkEventUpWithPPP);
  FRIEND_TEST(CellularTest, LinkEventUpWithoutPPP);
  FRIEND_TEST(CellularTest, LinkEventWontDestroyService);
  FRIEND_TEST(CellularTest, ModemStateChangeDisable);
  FRIEND_TEST(CellularTest, ModemStateChangeEnable);
  FRIEND_TEST(CellularTest, ModemStateChangeStaleConnected);
  FRIEND_TEST(CellularTest, ModemStateChangeValidConnected);
  FRIEND_TEST(CellularTest, Notify);
  FRIEND_TEST(CellularTest, OnAfterResumeDisableInProgressWantDisabled);
  FRIEND_TEST(CellularTest, OnAfterResumeDisableQueuedWantEnabled);
  FRIEND_TEST(CellularTest, OnAfterResumeDisabledWantDisabled);
  FRIEND_TEST(CellularTest, OnAfterResumeDisabledWantEnabled);
  FRIEND_TEST(CellularTest, OnAfterResumePowerDownInProgressWantEnabled);
  FRIEND_TEST(CellularTest, OnConnectionHealthCheckerResult);
  FRIEND_TEST(CellularTest, OnPPPDied);
  FRIEND_TEST(CellularTest, PPPConnectionFailedAfterAuth);
  FRIEND_TEST(CellularTest, PPPConnectionFailedBeforeAuth);
  FRIEND_TEST(CellularTest, PPPConnectionFailedDuringAuth);
  FRIEND_TEST(CellularTest, ScanAsynchronousFailure);
  FRIEND_TEST(CellularTest, ScanImmediateFailure);
  FRIEND_TEST(CellularTest, ScanSuccess);
  FRIEND_TEST(CellularTest, SetAllowRoaming);
  FRIEND_TEST(CellularTest, StartModemCallback);
  FRIEND_TEST(CellularTest, StartModemCallbackFail);
  FRIEND_TEST(CellularTest, StopModemCallback);
  FRIEND_TEST(CellularTest, StopModemCallbackFail);
  FRIEND_TEST(CellularTest, StopPPPOnDisconnect);
  FRIEND_TEST(CellularTest, StopPPPOnTermination);
  FRIEND_TEST(CellularTest, StartConnected);
  FRIEND_TEST(CellularTest, StartCDMARegister);
  FRIEND_TEST(CellularTest, StartGSMRegister);
  FRIEND_TEST(CellularTest, StartLinked);
  FRIEND_TEST(CellularTest, StartPPP);
  FRIEND_TEST(CellularTest, StartPPPAfterEthernetUp);
  FRIEND_TEST(CellularTest, StartPPPAlreadyStarted);
  FRIEND_TEST(CellularTest, UpdateScanning);
  FRIEND_TEST(Modem1Test, CreateDeviceMM1);

  // Names of properties in storage
  static const char kAllowRoaming[];

  // the |kScanningProperty| exposed by Cellular device is sticky false. Every
  // time it is set to true, it must be reset to false after a time equal to
  // this constant.
  static const int64 kDefaultScanningTimeoutMilliseconds;

  void SetState(State state);

  // Invoked when the modem is connected to the cellular network to transition
  // to the network-connected state and bring the network interface up.
  void EstablishLink();

  void InitCapability(Type type);

  void CreateService();

  // HelpRegisterDerived*: Expose a property over RPC, with the name |name|.
  //
  // Reads of the property will be handled by invoking |get|.
  // Writes to the property will be handled by invoking |set|.
  // Clearing the property will be handled by PropertyStore.
  void HelpRegisterDerivedBool(
      const std::string &name,
      bool(Cellular::*get)(Error *error),
      bool(Cellular::*set)(const bool &value, Error *error));
  void HelpRegisterConstDerivedString(
      const std::string &name,
      std::string(Cellular::*get)(Error *error));

  void OnConnectReply(const Error &error);
  void OnDisconnectReply(const Error &error);

  // DBUS accessors to read/modify the allow roaming property
  bool GetAllowRoaming(Error */*error*/) { return allow_roaming_; }
  bool SetAllowRoaming(const bool &value, Error *error);

  // When shill terminates or ChromeOS suspends, this function is called to
  // disconnect from the cellular network.
  void StartTermination();

  // This method is invoked upon the completion of StartTermination().
  void OnTerminationCompleted(const Error &error);

  // This function does the final cleanup once a disconnect request terminates.
  // Returns true, if the device state is successfully changed.
  bool DisconnectCleanup();

  // Executed after the asynchronous CellularCapability::StartModem
  // call from OnAfterResume completes.
  static void LogRestartModemResult(const Error &error);

  // Terminate the pppd process associated with this Device, and remove the
  // association between the PPPDevice and our CellularService. If this
  // Device is not using PPP, the method has no effect.
  void StopPPP();

  // Handlers for PPP events. Dispatched from Notify().
  void OnPPPAuthenticated();
  void OnPPPAuthenticating();
  void OnPPPConnected(const std::map<std::string, std::string> &params);
  void OnPPPDisconnected();

  void UpdateScanning();

  base::WeakPtrFactory<Cellular> weak_ptr_factory_;

  State state_;
  ModemState modem_state_;

  scoped_ptr<CellularCapability> capability_;

  // All DBus Properties exposed by the Cellular device.
  // Properties common to GSM and CDMA modems.
  const std::string dbus_owner_;  // :x.y
  const std::string dbus_service_;  // org.*.ModemManager*
  const std::string dbus_path_;  // ModemManager.Modem
  Operator home_provider_;

  bool scanning_supported_;
  std::string carrier_;
  std::string esn_;
  std::string firmware_revision_;
  std::string hardware_revision_;
  std::string imei_;
  std::string imsi_;
  std::string manufacturer_;
  std::string mdn_;
  std::string meid_;
  std::string min_;
  std::string model_id_;
  bool scanning_;

  // GSM only properties.
  // They are always exposed but are non empty only for GSM technology modems.
  std::string selected_network_;
  Stringmaps found_networks_;
  bool provider_requires_roaming_;
  uint16 scan_interval_;
  bool sim_present_;
  Stringmaps apn_list_;


  ModemInfo *modem_info_;
  ProxyFactory *proxy_factory_;
  PPPDeviceFactory *ppp_device_factory_;

  CellularServiceRefPtr service_;

  // User preference to allow or disallow roaming
  bool allow_roaming_;

  // Track whether a user initiated scan is in prgoress (initiated via ::Scan)
  bool proposed_scan_in_progress_;

  // Flag indicating that a disconnect has been explicitly requested.
  bool explicit_disconnect_;

  scoped_ptr<ExternalTask> ppp_task_;
  PPPDeviceRefPtr ppp_device_;
  bool is_ppp_authenticating_;

  // Sometimes modems may be stuck in the SEARCHING state during the lack of
  // presence of a network. During this indefinite duration of time, keeping
  // the Device.Scanning property as |true| causes a bad user experience.
  // This callback sets it to |false| after a timeout period has passed.
  base::CancelableClosure scanning_timeout_callback_;
  int64 scanning_timeout_milliseconds_;

  DISALLOW_COPY_AND_ASSIGN(Cellular);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_H_
