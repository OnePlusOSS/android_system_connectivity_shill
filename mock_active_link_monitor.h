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

#ifndef SHILL_MOCK_ACTIVE_LINK_MONITOR_H_
#define SHILL_MOCK_ACTIVE_LINK_MONITOR_H_

#include "shill/active_link_monitor.h"

#include <base/macros.h>
#include <gmock/gmock.h>

namespace shill {

class Connection;

class MockActiveLinkMonitor : public ActiveLinkMonitor {
 public:
  MockActiveLinkMonitor();
  ~MockActiveLinkMonitor() override;

  MOCK_METHOD1(Start, bool(int));
  MOCK_METHOD0(Stop, void());
  MOCK_CONST_METHOD0(gateway_mac_address, const ByteString&());
  MOCK_METHOD1(set_gateway_mac_address, void(const ByteString&));
  MOCK_CONST_METHOD0(gateway_supports_unicast_arp, bool());
  MOCK_METHOD1(set_gateway_supports_unicast_arp, void(bool));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockActiveLinkMonitor);
};

}  // namespace shill

#endif  // SHILL_MOCK_ACTIVE_LINK_MONITOR_H_
