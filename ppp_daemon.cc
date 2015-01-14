// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/ppp_daemon.h"

#include <map>
#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/memory/weak_ptr.h>

#include "shill/control_interface.h"
#include "shill/error.h"
#include "shill/external_task.h"
#include "shill/glib.h"
#include "shill/ppp_device.h"

namespace shill {

const char PPPDaemon::kDaemonPath[] = "/usr/sbin/pppd";
const char PPPDaemon::kPluginPath[] = SHIMDIR "/shill-pppd-plugin.so";

std::unique_ptr<ExternalTask> PPPDaemon::Start(
    ControlInterface *control_interface,
    GLib *glib,
    const base::WeakPtr<RPCTaskDelegate> &task_delegate,
    const PPPDaemon::Options &options,
    const std::string &device,
    const PPPDaemon::DeathCallback &death_callback,
    Error *error) {
  std::vector<std::string> arguments;
  if (options.no_detach) {
    arguments.push_back("nodetach");
  }
  if (options.no_default_route) {
    arguments.push_back("nodefaultroute");
  }
  if (options.use_peer_dns) {
    arguments.push_back("usepeerdns");
  }
  arguments.push_back("plugin");
  arguments.push_back(kPluginPath);
  arguments.push_back(device);

  std::unique_ptr<ExternalTask> task(new ExternalTask(
      control_interface, glib, task_delegate, death_callback));

  std::map<std::string, std::string> environment;
  if (task->Start(base::FilePath(kDaemonPath), arguments, environment, true,
                  error)) {
    return std::move(task);
  }
  return nullptr;
}

}  // namespace shill
