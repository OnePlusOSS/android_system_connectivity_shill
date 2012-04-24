// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/scope_logger.h"

#include <vector>

#include <base/string_tokenizer.h>
#include <base/string_util.h>

using std::string;
using std::vector;

namespace shill {

namespace {

const int kDefaultVerboseLevel = 0;

// Scope names corresponding to the scope defined by ScopeLogger::Scope.
const char *const kScopeNames[] = {
  "cellular",
  "connection",
  "crypto",
  "daemon",
  "dbus",
  "device",
  "dhcp",
  "dns",
  "ethernet",
  "http",
  "httpproxy",
  "inet",
  "manager",
  "metrics",
  "modem",
  "portal",
  "power",
  "profile",
  "property",
  "resolver",
  "route",
  "rtnl",
  "service",
  "storage",
  "task",
  "vpn",
  "wifi",
};

COMPILE_ASSERT(arraysize(kScopeNames) == ScopeLogger::kNumScopes,
               scope_tags_does_not_have_expected_number_of_strings);

// ScopeLogger needs to be a 'leaky' singleton as it needs to survive to
// handle logging till the very end of the shill process. Making ScopeLogger
// leaky is fine as it does not need to clean up or release any resource at
// destruction.
base::LazyInstance<ScopeLogger>::Leaky g_scope_logger =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
ScopeLogger* ScopeLogger::GetInstance() {
  return g_scope_logger.Pointer();
}

ScopeLogger::ScopeLogger()
    : verbose_level_(kDefaultVerboseLevel) {
}

ScopeLogger::~ScopeLogger() {
}

bool ScopeLogger::IsLogEnabled(Scope scope, int verbose_level) const {
  CHECK_GE(scope, 0);
  CHECK_LT(scope, kNumScopes);

  return scope_enabled_[scope] && verbose_level <= verbose_level_;
}

string ScopeLogger::GetAllScopeNames() const {
  vector<string> names(kScopeNames, kScopeNames + arraysize(kScopeNames));
  return JoinString(names, '+');
}

string ScopeLogger::GetEnabledScopeNames() const {
  vector<string> names;
  for (size_t i = 0; i < arraysize(kScopeNames); ++i) {
    if (scope_enabled_[i])
      names.push_back(kScopeNames[i]);
  }
  return JoinString(names, '+');
}

void ScopeLogger::EnableScopesByName(const string &expression) {
  if (expression.empty()) {
    DisableAllScopes();
    return;
  }

  // As described in the header file, if the first scope name in the
  // sequence specified by |expression| is not prefixed by a plus or
  // minus sign, it indicates that all scopes are first disabled before
  // enabled by |expression|.
  if (expression[0] != '+' && expression[0] != '-')
    DisableAllScopes();

  bool enable_scope = true;
  StringTokenizer tokenizer(expression, "+-");
  tokenizer.set_options(StringTokenizer::RETURN_DELIMS);
  while (tokenizer.GetNext()) {
    if (tokenizer.token_is_delim()) {
      enable_scope = (tokenizer.token() == "+");
      continue;
    }

    if (tokenizer.token().empty())
      continue;

    size_t i;
    for (i = 0; i < arraysize(kScopeNames); ++i) {
      if (tokenizer.token() == kScopeNames[i]) {
        SetScopeEnabled(static_cast<Scope>(i), enable_scope);
        break;
      }
    }
    LOG_IF(WARNING, i == arraysize(kScopeNames))
        << "Unknown scope '" << tokenizer.token() << "'";
  }
}

void ScopeLogger::DisableAllScopes() {
  scope_enabled_.reset();
}

void ScopeLogger::SetScopeEnabled(Scope scope, bool enabled) {
  CHECK_GE(scope, 0);
  CHECK_LT(scope, kNumScopes);

  scope_enabled_[scope] = enabled;
}

}  // namespace shill
