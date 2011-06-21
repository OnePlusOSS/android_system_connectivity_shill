// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dhcp_provider.h"
#include "shill/mock_glib.h"

using testing::Test;

namespace shill {

namespace {
const char kDeviceName[] = "testdevicename";
}  // namespace {}

class DHCPProviderTest : public Test {
 public:
  DHCPProviderTest() : provider_(DHCPProvider::GetInstance()) {
    provider_->glib_ = &glib_;
  }

 protected:
  MockGLib glib_;
  DHCPProvider *provider_;
};

TEST_F(DHCPProviderTest, CreateConfig) {
  DHCPConfigRefPtr config = provider_->CreateConfig(kDeviceName);
  EXPECT_TRUE(config.get());
  EXPECT_EQ(&glib_, config->glib_);
  EXPECT_EQ(kDeviceName, config->device_name());
  EXPECT_TRUE(provider_->configs_.empty());
}

}  // namespace shill
