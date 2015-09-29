#
# Copyright (C) 2015 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

LOCAL_PATH := $(call my-dir)

# Common variables
# ========================================================

# Definitions applying to all targets built from C++ source files.
# Be sure to $(eval) this last.
define shill_cpp_common
  LOCAL_CPP_EXTENSION := .cc
  LOCAL_CLANG := true
  LOCAL_RTTI_FLAG := -frtti
  LOCAL_CFLAGS := \
    -Wextra \
    -Werror \
    -Wno-unused-parameter \
    -DRUNDIR=\"/data/misc/shill\" \
    -DSHIMDIR=\"/system/lib/shill/shims\" \
    -DDISABLE_CELLULAR \
    -DDISABLE_VPN \
    -DDISABLE_WAKE_ON_WIFI \
    -DDISABLE_WIMAX \
    -DENABLE_CHROMEOS_DBUS \
    -DENABLE_JSON_STORE
  ifneq ($(shill_use_dhcpv6), yes)
    LOCAL_CFLAGS += -DDISABLE_DHCPV6
  endif
  ifneq ($(shill_use_pppoe), yes)
    LOCAL_CFLAGS += -DDISABLE_PPPOE
  endif
  ifneq ($(shill_use_wired_8021x), yes)
    LOCAL_CFLAGS += -DDISABLE_WIRED_8021X
  endif
  ifdef BRILLO
    LOCAL_CFLAGS += -D__BRILLO__
  endif
endef

shill_parent_dir := $(LOCAL_PATH)/../

shill_c_includes := \
    $(shill_parent_dir) \
    external/gtest/include/

shill_shared_libraries := \
    libchrome \
    libchromeos \
    libdbus

shill_cpp_flags := \
    -fno-strict-aliasing \
    -Woverloaded-virtual \
    -Wno-sign-promo \
    -Wno-missing-field-initializers  # for LAZY_INSTANCE_INITIALIZER

# libshill-net (shared library)
# ========================================================
include $(CLEAR_VARS)
LOCAL_MODULE := libshill-net
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_CPPFLAGS := $(shill_cpp_flags)
LOCAL_SHARED_LIBRARIES := $(shill_shared_libraries)
LOCAL_C_INCLUDES := $(shill_c_includes)
LOCAL_SRC_FILES := \
    net/attribute_list.cc \
    net/byte_string.cc \
    net/control_netlink_attribute.cc \
    net/event_history.cc \
    net/generic_netlink_message.cc \
    net/io_handler_factory.cc \
    net/io_handler_factory_container.cc \
    net/io_input_handler.cc \
    net/io_ready_handler.cc \
    net/ip_address.cc \
    net/netlink_attribute.cc \
    net/netlink_manager.cc \
    net/netlink_message.cc \
    net/netlink_packet.cc \
    net/netlink_socket.cc \
    net/nl80211_attribute.cc \
    net/nl80211_message.cc \
    net/rtnl_handler.cc \
    net/rtnl_listener.cc \
    net/rtnl_message.cc \
    net/shill_time.cc \
    net/sockets.cc
ifdef BRILLO
LOCAL_SHARED_LIBRARIES += libhardware
LOCAL_C_INCLUDES += device/generic/brillo/wifi_driver_hal/include
LOCAL_REQUIRED_MODULES := $(WIFI_DRIVER_HAL_MODULE)
LOCAL_SRC_FILES += net/wifi_driver_hal.cc
endif # BRILLO
$(eval $(shill_cpp_common))
include $(BUILD_SHARED_LIBRARY)

# libshill-client (shared library)
# ========================================================
include $(CLEAR_VARS)
LOCAL_MODULE := libshill-client
LOCAL_DBUS_PROXY_PREFIX := shill
LOCAL_SRC_FILES := \
    dbus_bindings/dbus-service-config.json \
    dbus_bindings/org.chromium.flimflam.Device.dbus-xml \
    dbus_bindings/org.chromium.flimflam.IPConfig.dbus-xml \
    dbus_bindings/org.chromium.flimflam.Manager.dbus-xml \
    dbus_bindings/org.chromium.flimflam.Profile.dbus-xml \
    dbus_bindings/org.chromium.flimflam.Service.dbus-xml \
    dbus_bindings/org.chromium.flimflam.Task.dbus-xml \
    dbus_bindings/org.chromium.flimflam.ThirdPartyVpn.dbus-xml
include $(BUILD_SHARED_LIBRARY)

# supplicant-proxies (static library)
# ========================================================
include $(CLEAR_VARS)
LOCAL_MODULE := supplicant-proxies
LOCAL_DBUS_PROXY_PREFIX := supplicant
LOCAL_SRC_FILES := \
    dbus_bindings/supplicant-bss.dbus-xml \
    dbus_bindings/supplicant-interface.dbus-xml \
    dbus_bindings/supplicant-network.dbus-xml \
    dbus_bindings/supplicant-process.dbus-xml
include $(BUILD_STATIC_LIBRARY)

# dhcpcd-proxies (static library)
# ========================================================
include $(CLEAR_VARS)
LOCAL_MODULE := dhcpcd-proxies
LOCAL_DBUS_PROXY_PREFIX := dhcpcd
LOCAL_SRC_FILES := dbus_bindings/dhcpcd.dbus-xml
include $(BUILD_STATIC_LIBRARY)

# libshill (static library)
# ========================================================
include $(CLEAR_VARS)
LOCAL_MODULE := libshill
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_CPPFLAGS := $(shill_cpp_flags) -Wno-sign-compare
LOCAL_STATIC_LIBRARIES := \
    supplicant-proxies \
    dhcpcd-proxies
LOCAL_SHARED_LIBRARIES := \
    $(shill_shared_libraries) \
    libshill-net \
    libcares \
    libmetrics \
    libprotobuf-cpp-lite \
    libminijail \
    libfirewalld-client
proto_header_dir := $(call local-generated-sources-dir)/proto/$(shill_parent_dir)
LOCAL_C_INCLUDES := \
    $(shill_c_includes) \
    $(proto_header_dir) \
    external/cros/system_api/
LOCAL_SRC_FILES := \
    shims/protos/crypto_util.proto \
    dbus_bindings/org.chromium.flimflam.Device.dbus-xml \
    dbus_bindings/org.chromium.flimflam.IPConfig.dbus-xml \
    dbus_bindings/org.chromium.flimflam.Manager.dbus-xml \
    dbus_bindings/org.chromium.flimflam.Profile.dbus-xml \
    dbus_bindings/org.chromium.flimflam.Service.dbus-xml \
    dbus_bindings/org.chromium.flimflam.Task.dbus-xml \
    dbus_bindings/org.chromium.flimflam.ThirdPartyVpn.dbus-xml \
    json_store.cc \
    wifi/callback80211_metrics.cc \
    wifi/mac80211_monitor.cc \
    wifi/scan_session.cc \
    wifi/tdls_manager.cc \
    wifi/wake_on_wifi.cc \
    wifi/wifi.cc \
    wifi/wifi_endpoint.cc \
    wifi/wifi_provider.cc \
    wifi/wifi_service.cc \
    dbus/chromeos_supplicant_bss_proxy.cc \
    dbus/chromeos_supplicant_interface_proxy.cc \
    dbus/chromeos_supplicant_network_proxy.cc \
    dbus/chromeos_supplicant_process_proxy.cc \
    eap_credentials.cc \
    eap_listener.cc \
    supplicant/supplicant_eap_state_handler.cc \
    supplicant/wpa_supplicant.cc \
    active_link_monitor.cc \
    arp_client.cc \
    arp_packet.cc \
    async_connection.cc \
    certificate_file.cc \
    chromeos_daemon.cc \
    connection.cc \
    connection_diagnostics.cc \
    connection_health_checker.cc \
    connection_info.cc \
    connection_info_reader.cc \
    connection_tester.cc \
    connectivity_trial.cc \
    crypto_rot47.cc \
    crypto_util_proxy.cc \
    dbus/chromeos_dbus_adaptor.cc \
    dbus/chromeos_dbus_control.cc \
    dbus/chromeos_dbus_daemon.cc \
    dbus/chromeos_dbus_service_watcher.cc \
    dbus/chromeos_device_dbus_adaptor.cc \
    dbus/chromeos_dhcpcd_listener.cc \
    dbus/chromeos_dhcpcd_proxy.cc \
    dbus/chromeos_firewalld_proxy.cc \
    dbus/chromeos_ipconfig_dbus_adaptor.cc \
    dbus/chromeos_manager_dbus_adaptor.cc \
    dbus/chromeos_profile_dbus_adaptor.cc \
    dbus/chromeos_rpc_task_dbus_adaptor.cc \
    dbus/chromeos_service_dbus_adaptor.cc \
    dbus/chromeos_third_party_vpn_dbus_adaptor.cc \
    default_profile.cc \
    device.cc \
    device_claimer.cc \
    device_info.cc \
    dhcp/dhcp_config.cc \
    dhcp/dhcp_provider.cc \
    dhcp/dhcpv4_config.cc \
    dns_client.cc \
    dns_client_factory.cc \
    dns_server_tester.cc \
    ephemeral_profile.cc \
    error.cc \
    ethernet/ethernet.cc \
    ethernet/ethernet_service.cc \
    ethernet/ethernet_temporary_service.cc \
    ethernet/virtio_ethernet.cc \
    event_dispatcher.cc \
    external_task.cc \
    file_io.cc \
    file_reader.cc \
    geolocation_info.cc \
    hook_table.cc \
    http_proxy.cc \
    http_request.cc \
    http_url.cc \
    icmp.cc \
    icmp_session.cc \
    icmp_session_factory.cc \
    ip_address_store.cc \
    ipconfig.cc \
    key_value_store.cc \
    link_monitor.cc \
    logging.cc \
    manager.cc \
    metrics.cc \
    passive_link_monitor.cc \
    pending_activation_store.cc \
    portal_detector.cc \
    power_manager.cc \
    power_manager_proxy_stub.cc \
    ppp_daemon.cc \
    ppp_device.cc \
    ppp_device_factory.cc \
    pppoe/pppoe_service.cc \
    process_manager.cc \
    profile.cc \
    property_store.cc \
    resolver.cc \
    result_aggregator.cc \
    routing_table.cc \
    rpc_task.cc \
    scope_logger.cc \
    scoped_umask.cc \
    service.cc \
    service_property_change_notifier.cc \
    shill_ares.cc \
    shill_config.cc \
    shill_test_config.cc \
    socket_info.cc \
    socket_info_reader.cc \
    static_ip_parameters.cc \
    store_factory.cc \
    technology.cc \
    tethering.cc \
    traffic_monitor.cc \
    upstart/upstart.cc \
    upstart/upstart_proxy_stub.cc \
    virtual_device.cc \
    vpn/vpn_driver.cc \
    vpn/vpn_provider.cc \
    vpn/vpn_service.cc
ifeq ($(shill_use_wired_8021x), yes)
LOCAL_SRC_FILES += \
    ethernet/ethernet_eap_provider.cc \
    ethernet/ethernet_eap_service.cc
endif
ifeq ($(shill_use_dhcpv6), yes)
LOCAL_SRC_FILES += dhcp/dhcpv6_config.cc
endif
$(eval $(shill_cpp_common))
include $(BUILD_STATIC_LIBRARY)

# shill
# ========================================================
include $(CLEAR_VARS)
LOCAL_MODULE := shill
LOCAL_CPPFLAGS := $(shill_cpp_flags)
LOCAL_SHARED_LIBRARIES := \
    $(shill_shared_libraries) \
    libchromeos-minijail \
    libminijail \
    libcares \
    libchromeos-dbus \
    libchrome-dbus \
    libshill-net \
    libmetrics \
    libprotobuf-cpp-lite-rtti
LOCAL_STATIC_LIBRARIES := libshill
LOCAL_C_INCLUDES := $(shill_c_includes)
LOCAL_SRC_FILES := shill_main.cc
$(eval $(shill_cpp_common))
include $(BUILD_EXECUTABLE)

# shill_unittest (native test)
# ========================================================
include $(CLEAR_VARS)
LOCAL_MODULE := shill_unittest
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_CPPFLAGS := $(shill_cpp_flags) -Wno-sign-compare -Wno-return-stack-address
LOCAL_SHARED_LIBRARIES := \
    $(shill_shared_libraries) \
    libshill-net \
    libminijail \
    libmetrics \
    libcares \
    libchromeos-minijail \
    libchromeos-dbus \
    libchrome-dbus \
    libprotobuf-cpp-lite-rtti
LOCAL_STATIC_LIBRARIES := libshill libgmock libchrome_test_helpers
proto_header_dir := $(call local-generated-sources-dir)/proto/$(shill_parent_dir)
LOCAL_C_INCLUDES := \
    $(shill_c_includes) \
    $(proto_header_dir) \
    external/cros/system_api/
LOCAL_SRC_FILES := \
    shims/protos/crypto_util.proto \
    active_link_monitor_unittest.cc \
    arp_client_test_helper.cc \
    arp_client_unittest.cc \
    arp_packet_unittest.cc \
    async_connection_unittest.cc \
    certificate_file_unittest.cc \
    chromeos_daemon_unittest.cc \
    connection_diagnostics_unittest.cc \
    connection_health_checker_unittest.cc \
    connection_info_reader_unittest.cc \
    connection_info_unittest.cc \
    connection_tester_unittest.cc \
    connection_unittest.cc \
    connectivity_trial_unittest.cc \
    crypto_rot47_unittest.cc \
    crypto_util_proxy_unittest.cc \
    dbus/chromeos_dbus_adaptor_unittest.cc \
    default_profile_unittest.cc \
    device_claimer_unittest.cc \
    device_info_unittest.cc \
    device_unittest.cc \
    dhcp/dhcp_config_unittest.cc \
    dhcp/dhcp_provider_unittest.cc \
    dhcp/dhcpv4_config_unittest.cc \
    dhcp/mock_dhcp_config.cc \
    dhcp/mock_dhcp_provider.cc \
    dhcp/mock_dhcp_proxy.cc \
    dns_client_unittest.cc \
    dns_server_tester_unittest.cc \
    error_unittest.cc \
    ethernet/ethernet_service_unittest.cc \
    ethernet/ethernet_unittest.cc \
    ethernet/mock_ethernet.cc \
    ethernet/mock_ethernet_service.cc \
    external_task_unittest.cc \
    fake_store.cc \
    file_reader_unittest.cc \
    hook_table_unittest.cc \
    http_proxy_unittest.cc \
    http_request_unittest.cc \
    http_url_unittest.cc \
    icmp_unittest.cc \
    icmp_session_unittest.cc \
    ip_address_store_unittest.cc \
    ipconfig_unittest.cc \
    key_value_store_unittest.cc \
    link_monitor_unittest.cc \
    manager_unittest.cc \
    metrics_unittest.cc \
    mock_active_link_monitor.cc \
    mock_adaptors.cc \
    mock_ares.cc \
    mock_arp_client.cc \
    mock_async_connection.cc \
    mock_certificate_file.cc \
    mock_connection.cc \
    mock_connection_health_checker.cc \
    mock_connection_info_reader.cc \
    mock_connectivity_trial.cc \
    mock_control.cc \
    mock_crypto_util_proxy.cc \
    mock_device.cc \
    mock_device_claimer.cc \
    mock_device_info.cc \
    mock_dns_client.cc \
    mock_dns_client_factory.cc \
    mock_dns_server_tester.cc \
    mock_event_dispatcher.cc \
    mock_external_task.cc \
    mock_http_request.cc \
    mock_icmp.cc \
    mock_icmp_session.cc \
    mock_icmp_session_factory.cc \
    mock_ip_address_store.cc \
    mock_ipconfig.cc \
    mock_link_monitor.cc \
    mock_log.cc \
    mock_log_unittest.cc \
    mock_manager.cc \
    mock_metrics.cc \
    mock_passive_link_monitor.cc \
    mock_pending_activation_store.cc \
    mock_portal_detector.cc \
    mock_power_manager.cc \
    mock_power_manager_proxy.cc \
    mock_ppp_device.cc \
    mock_ppp_device_factory.cc \
    mock_process_manager.cc \
    mock_profile.cc \
    mock_property_store.cc \
    mock_resolver.cc \
    mock_routing_table.cc \
    mock_service.cc \
    mock_socket_info_reader.cc \
    mock_store.cc \
    mock_traffic_monitor.cc \
    mock_virtual_device.cc \
    net/attribute_list_unittest.cc \
    net/byte_string_unittest.cc \
    net/event_history_unittest.cc \
    net/ip_address_unittest.cc \
    net/netlink_attribute_unittest.cc \
    net/rtnl_handler_unittest.cc \
    net/rtnl_listener_unittest.cc \
    net/rtnl_message_unittest.cc \
    net/shill_time_unittest.cc \
    nice_mock_control.cc \
    passive_link_monitor_unittest.cc \
    pending_activation_store_unittest.cc \
    portal_detector_unittest.cc \
    power_manager_unittest.cc \
    ppp_daemon_unittest.cc \
    ppp_device_unittest.cc \
    pppoe/pppoe_service_unittest.cc \
    process_manager_unittest.cc \
    profile_unittest.cc \
    property_accessor_unittest.cc \
    property_observer_unittest.cc \
    property_store_unittest.cc \
    resolver_unittest.cc \
    result_aggregator_unittest.cc \
    routing_table_unittest.cc \
    rpc_task_unittest.cc \
    scope_logger_unittest.cc \
    service_property_change_test.cc \
    service_under_test.cc \
    service_unittest.cc \
    socket_info_reader_unittest.cc \
    socket_info_unittest.cc \
    static_ip_parameters_unittest.cc \
    technology_unittest.cc \
    testrunner.cc \
    traffic_monitor_unittest.cc \
    upstart/mock_upstart.cc \
    upstart/mock_upstart_proxy.cc \
    upstart/upstart_unittest.cc \
    virtual_device_unittest.cc \
    vpn/mock_vpn_provider.cc \
    json_store_unittest.cc \
    net/netlink_manager_unittest.cc \
    net/netlink_message_unittest.cc \
    net/netlink_packet_unittest.cc \
    net/netlink_socket_unittest.cc \
    supplicant/mock_supplicant_bss_proxy.cc \
    wifi/callback80211_metrics_unittest.cc \
    wifi/mac80211_monitor_unittest.cc \
    wifi/mock_mac80211_monitor.cc \
    wifi/mock_scan_session.cc \
    wifi/mock_tdls_manager.cc \
    wifi/mock_wake_on_wifi.cc \
    wifi/mock_wifi.cc \
    wifi/mock_wifi_provider.cc \
    wifi/mock_wifi_service.cc \
    wifi/scan_session_unittest.cc \
    wifi/tdls_manager_unittest.cc \
    wifi/wake_on_wifi_unittest.cc \
    wifi/wifi_endpoint_unittest.cc \
    wifi/wifi_provider_unittest.cc \
    wifi/wifi_service_unittest.cc \
    wifi/wifi_unittest.cc \
    eap_credentials_unittest.cc \
    eap_listener_unittest.cc \
    mock_eap_credentials.cc \
    mock_eap_listener.cc \
    supplicant/mock_supplicant_eap_state_handler.cc \
    supplicant/mock_supplicant_interface_proxy.cc \
    supplicant/mock_supplicant_network_proxy.cc \
    supplicant/mock_supplicant_process_proxy.cc \
    supplicant/supplicant_eap_state_handler_unittest.cc \
    supplicant/wpa_supplicant_unittest.cc
ifeq ($(shill_use_dhcpv6), yes)
LOCAL_SRC_FILES += dhcp/dhcpv6_config_unittest.cc
endif
ifeq ($(shill_use_wired_8021x), yes)
LOCAL_SRC_FILES += \
    ethernet/ethernet_eap_provider_unittest.cc \
    ethernet/ethernet_eap_service_unittest.cc \
    ethernet/mock_ethernet_eap_provider.cc
endif
$(eval $(shill_cpp_common))
include $(BUILD_NATIVE_TEST)
