// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include <iostream>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "extension_creator.h"


int main(int argc, char **argv){
  base::CommandLine::Init(argc, argv);

  logging::LoggingSettings logging_settings;
  logging_settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  logging::InitLogging(logging_settings);

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  base::FilePath extension_path =
      command_line->GetSwitchValuePath("extension-path");

  base::FilePath crx_path = command_line->GetSwitchValuePath("crx-path");

  base::FilePath pem_path = command_line->GetSwitchValuePath("pem-path");

  extensions::ExtensionCreator::RunFlags run_flags;
  run_flags = static_cast<extensions::ExtensionCreator::RunFlags>(
      extensions::ExtensionCreator::kOverwriteCRX
      | extensions::ExtensionCreator::kRequireModernManifestVersion);

  extensions::ExtensionCreator creator;
  if (!creator.Run(extension_path, crx_path, pem_path, run_flags)) {
    std::cout << "crx package failed: " << creator.error_message() << std::endl;
    return 1;
  }
  std::cout << "crx package success" << std::endl;

  return 0;
}
