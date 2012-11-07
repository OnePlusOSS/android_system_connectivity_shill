// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/callback80211_object.h"

#include <string>

#include <base/memory/weak_ptr.h>
#include <base/stringprintf.h>

#include "shill/config80211.h"
#include "shill/ieee80211.h"
#include "shill/link_monitor.h"
#include "shill/logging.h"
#include "shill/scope_logger.h"
#include "shill/user_bound_nlmessage.h"

using base::Bind;
using base::StringAppendF;
using std::string;

namespace shill {

Callback80211Object::Callback80211Object(Config80211 *config80211)
    : weak_ptr_factory_(this),
      callback_(Bind(&Callback80211Object::Config80211MessageCallback,
                     weak_ptr_factory_.GetWeakPtr())),
      config80211_(config80211) {
}

Callback80211Object::~Callback80211Object() {
  DeinstallAsCallback();
}

void Callback80211Object::Config80211MessageCallback(
    const UserBoundNlMessage &message) {
  // Show the simplified version of the message.
  string output("@");
  StringAppendF(&output, "%s", message.ToString().c_str());
  SLOG(WiFi, 2) << output;

  // Show the more complicated version of the message.
  SLOG(WiFi, 3) << "Received " << message.GetMessageTypeString()
                << " (" << + message.GetMessageType() << ")";

  scoped_ptr<UserBoundNlMessage::AttributeNameIterator> i;
  for (i.reset(message.GetAttributeNameIterator()); !i->AtEnd(); i->Advance()) {
    string value = "<unknown>";
    message.GetAttributeString(i->GetName(), &value);
    SLOG(WiFi, 3) << "   Attr:" << message.StringFromAttributeName(i->GetName())
                  << "=" << value
                  << " Type:" << message.GetAttributeTypeString(i->GetName());
  }
}

bool Callback80211Object::InstallAsBroadcastCallback() {
  if (config80211_) {
    return config80211_->AddBroadcastCallback(callback_);
  }
  return false;
}

bool Callback80211Object::DeinstallAsCallback() {
  if (config80211_) {
    config80211_->RemoveBroadcastCallback(callback_);
    return true;
  }
  return false;
}

}  // namespace shill.
