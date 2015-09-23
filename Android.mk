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

LOCAL_PATH := $(call my-dir)

ifeq ($(HOST_OS),linux)

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
  ifneq ($(shill_use_dhcpv6)), yes)
    LOCAL_CFLAGS += -DDISABLE_DHCPV6
  endif
  ifneq ($(shill_use_pppoe)), yes)
    LOCAL_CFLAGS += -DDISABLE_PPPOE
  endif
  ifneq ($(shill_use_wired_8021x)), yes)
    LOCAL_CFLAGS += -DDISABLE_WIRED_8021X
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
ifeq ($(shill_use_wired_8021x)), yes)
LOCAL_SRC_FILES += \
    ethernet/ethernet_eap_provider.cc \
    ethernet/ethernet_eap_service.cc
endif
ifeq ($(shill_use_dhcpv6)), yes)
LOCAL_SRC_FILES += dhcp/dhcpv6_config.cc
endif
$(eval $(shill_cpp_common))
include $(BUILD_STATIC_LIBRARY)

endif # HOST_OS == linux
