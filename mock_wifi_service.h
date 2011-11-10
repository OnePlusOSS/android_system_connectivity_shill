// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_WIFI_SERVICE_
#define SHILL_MOCK_WIFI_SERVICE_

#include <gmock/gmock.h>

#include "shill/wifi_service.h"

namespace shill {

class MockWiFiService : public WiFiService {
 public:
  MockWiFiService(ControlInterface *control_interface,
                  EventDispatcher *dispatcher,
                  Manager *manager,
                  const WiFiRefPtr &device,
                  const std::vector<uint8_t> &ssid,
                  const std::string &mode,
                  const std::string &security,
                  bool hidden_ssid);
  virtual ~MockWiFiService();

  MOCK_METHOD1(SetState, void(ConnectState state));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWiFiService);
};

}  // namespace shill

#endif  // SHILL_MOCK_WIFI_SERVICE_
