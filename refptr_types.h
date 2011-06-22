// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_REFPTR_TYPES_
#define SHILL_REFPTR_TYPES_

#include <base/memory/ref_counted.h>

namespace shill {

// Device and derived classes.
class Device;
typedef scoped_refptr<const Device> DeviceConstRefPtr;
typedef scoped_refptr<Device> DeviceRefPtr;

class Cellular;
typedef scoped_refptr<const Cellular> CellularConstRefPtr;
typedef scoped_refptr<Cellular> CellularRefPtr;

class Ethernet;
typedef scoped_refptr<const Ethernet> EthernetConstRefPtr;
typedef scoped_refptr<Ethernet> EthernetRefPtr;

class WiFi;
typedef scoped_refptr<const WiFi> WiFiConstRefPtr;
typedef scoped_refptr<WiFi> WiFiRefPtr;

// Endpoint and derived classes.
class Endpoint;
typedef scoped_refptr<const Endpoint> EndpointConstRefPtr;
typedef scoped_refptr<Endpoint> EndpointRefPtr;

class WiFiEndpoint;
typedef scoped_refptr<const WiFiEndpoint> WiFiEndpointConstRefPtr;
typedef scoped_refptr<WiFiEndpoint> WiFiEndpointRefPtr;

// Service and derived classes.
class Service;
typedef scoped_refptr<const Service> ServiceConstRefPtr;
typedef scoped_refptr<Service> ServiceRefPtr;

class CellularService;
typedef scoped_refptr<const CellularService> CellularServiceConstRefPtr;
typedef scoped_refptr<CellularService> CellularServiceRefPtr;

class EthernetService;
typedef scoped_refptr<const EthernetService> EthernetServiceConstRefPtr;
typedef scoped_refptr<EthernetService> EthernetServiceRefPtr;

class WiFiService;
typedef scoped_refptr<const WiFiService> WiFiServiceConstRefPtr;
typedef scoped_refptr<WiFiService> WiFiServiceRefPtr;

// IPConfig and derived classes.
class IPConfig;
typedef scoped_refptr<IPConfig> IPConfigRefPtr;

class DHCPConfig;
typedef scoped_refptr<DHCPConfig> DHCPConfigRefPtr;

}  // namespace shill

#endif  // SHILL_REFPTR_TYPES_
