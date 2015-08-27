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

# Common variables
# ========================================================

shill_cpp_extension := .cc

shill_c_includes := \
    system/connectivity/ \
    external/gtest/include/

shill_shared_libraries := \
    libchrome \
    libchromeos \
    libdbus

shill_c_flags := \
    -Wextra \
    -Werror \
    -Wno-unused-parameter \
    -DRUNDIR="/data/misc/shill" \
    -DSHIMDIR="/system/lib/shill/shims" \
    -DDISABLE_CELLULAR \
    -DDISABLE_VPN \
    -DDISABLE_WAKE_ON_WIFI \
    -DDISABLE_WIMAX \
    -DENABLE_CHROMEOS_DBUS \
    -DENABLE_JSON_STORE

shill_cpp_flags := \
    -fno-strict-aliasing \
    -Woverloaded-virtual \
    -Wno-missing-field-initializers  # for LAZY_INSTANCE_INITIALIZER

ifeq ($(shill_use_dhcpv6)), yes)
else
shill_c_flags += -DDISABLE_DHCPV6
endif

ifeq ($(shill_use_pppoe)), yes)
else
shill_c_flags += -DDISABLE_PPPOE
endif

ifeq ($(shill_use_wired_8021x)), yes)
else
shill_c_flags += -DDISABLE_WIRED_8021X
endif

# libshill-net
# ========================================================
include $(CLEAR_VARS)
LOCAL_CPP_EXTENSION := $(shill_cpp_extension)
LOCAL_MODULE := libshill-net
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_SRC_FILES := $(libshill_net_sources)
LOCAL_CFLAGS := $(shill_c_flags)
LOCAL_CPPFLAGS := $(shill_cpp_flags) -Wno-sign-promo
LOCAL_RTTI_FLAG := -frtti
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
include $(BUILD_SHARED_LIBRARY)
