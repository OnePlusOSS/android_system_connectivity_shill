//
// Copyright (C) 2012 The Android Open Source Project
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

#ifndef SHILL_CELLULAR_MOCK_MODEM_GOBI_PROXY_H_
#define SHILL_CELLULAR_MOCK_MODEM_GOBI_PROXY_H_

#include <string>

#include <base/macros.h>
#include <gmock/gmock.h>

#include "shill/cellular/modem_gobi_proxy_interface.h"

namespace shill {

class MockModemGobiProxy : public ModemGobiProxyInterface {
 public:
  MockModemGobiProxy();
  ~MockModemGobiProxy() override;

  MOCK_METHOD4(SetCarrier, void(const std::string& carrier,
                                Error* error, const ResultCallback& callback,
                                int timeout));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockModemGobiProxy);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_MOCK_MODEM_GOBI_PROXY_H_
