// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extension_creator.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_util.h"
#include "components/crx_file/crx_creator.h"
#include "components/crx_file/id_util.h"
#include "crypto/rsa_private_key.h"
#include "crypto/signature_creator.h"
#include "extension_creator_filter.h"
#include "extensions/common/extension.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "zip.h"

namespace {

// KEY MARKERS
const char kKeyBeginHeaderMarker[] = "-----BEGIN";
const char kKeyBeginFooterMarker[] = "-----END";
const char kKeyInfoEndMarker[] = "KEY-----";

bool ParsePEMKeyBytes(const std::string& input,
                                 std::string* output) {
  DCHECK(output);
  if (!output)
    return false;
  if (input.length() == 0)
    return false;

  std::string working = input;
  if (base::StartsWith(working, kKeyBeginHeaderMarker,
                       base::CompareCase::SENSITIVE)) {
    working = base::CollapseWhitespaceASCII(working, true);
    size_t header_pos =
        working.find(kKeyInfoEndMarker, sizeof(kKeyBeginHeaderMarker) - 1);
    if (header_pos == std::string::npos)
      return false;
    size_t start_pos = header_pos + sizeof(kKeyInfoEndMarker) - 1;
    size_t end_pos = working.rfind(kKeyBeginFooterMarker);
    if (end_pos == std::string::npos)
      return false;
    if (start_pos >= end_pos)
      return false;

    working = working.substr(start_pos, end_pos - start_pos);
    if (working.length() == 0)
      return false;
  }

  return base::Base64Decode(working, output);
}

}

namespace extensions {

ExtensionCreator::ExtensionCreator() : error_type_(kOtherError) {}

std::unique_ptr<crypto::RSAPrivateKey> ExtensionCreator::ReadInputKey(
    const base::FilePath& private_key_path) {
  if (!base::PathExists(private_key_path)) {
    error_message_ = "PRIVATE_KEY_NO_EXISTS";
    return nullptr;
  }

  std::string private_key_contents;
  if (!base::ReadFileToString(private_key_path, &private_key_contents)) {
    error_message_ = "PRIVATE_KEY_FAILED_TO_READ";
    return nullptr;
  }

  std::string private_key_bytes;
  if (!ParsePEMKeyBytes(private_key_contents, &private_key_bytes)) {
    error_message_ = "PRIVATE_KEY_INVALID";
    return nullptr;
  }

  std::unique_ptr<crypto::RSAPrivateKey> private_key =
      crypto::RSAPrivateKey::CreateFromPrivateKeyInfo(std::vector<uint8_t>(
          private_key_bytes.begin(), private_key_bytes.end()));
  if (!private_key) {
    error_message_ = "PRIVATE_KEY_INVALID_FORMAT";
    return nullptr;
  }

  return private_key;
}

bool ExtensionCreator::CreateZip(const base::FilePath& extension_dir,
                                 const base::FilePath& temp_path,
                                 base::FilePath* zip_path) {
  *zip_path = temp_path.Append(FILE_PATH_LITERAL("extension.zip"));

  scoped_refptr<ExtensionCreatorFilter> filter =
      base::MakeRefCounted<ExtensionCreatorFilter>(extension_dir);
  zip::FilterCallback filter_cb =
      base::BindRepeating(&ExtensionCreatorFilter::ShouldPackageFile, filter);

  // TODO(crbug.com/862471): Surface a warning to the user for files excluded
  // from being packed.
  if (!zip::ZipWithFilterCallback(extension_dir, *zip_path,
                                  std::move(filter_cb))) {
    error_message_ = "FAILED_DURING_PACKAGING";
    return false;
  }

  return true;
}

bool ExtensionCreator::CreateCrx(
    const base::FilePath& zip_path,
    crypto::RSAPrivateKey* private_key,
    const base::FilePath& crx_path,
    const absl::optional<std::string>& compressed_verified_contents) {
  crx_file::CreatorResult result;
  if (compressed_verified_contents.has_value()) {
    result = crx_file::CreateCrxWithVerifiedContentsInHeader(
        crx_path, zip_path, private_key, compressed_verified_contents.value());
  } else {
    result = crx_file::Create(crx_path, zip_path, private_key);
  }
  switch (result) {
    case crx_file::CreatorResult::OK:
      return true;
    case crx_file::CreatorResult::ERROR_SIGNING_FAILURE:
      error_message_ = "ERROR_SIGNING_FAILURE";
      return false;
    case crx_file::CreatorResult::ERROR_FILE_NOT_WRITABLE:
      error_message_ = "ERROR_FILE_NOT_WRITABLE";
      return false;
    case crx_file::CreatorResult::ERROR_FILE_NOT_READABLE:
	  error_message_ = "ERROR_FILE_NOT_READABLE";
	  return false;
    case crx_file::CreatorResult::ERROR_FILE_WRITE_FAILURE:
	  error_message_ = "ERROR_FILE_WRITE_FAILURE";
      return false;
  }
  return false;
}

bool ExtensionCreator::CreateCrxAndPerformCleanup(
    const base::FilePath& extension_dir,
    const base::FilePath& crx_path,
    crypto::RSAPrivateKey* private_key,
    const absl::optional<std::string>& compressed_verified_contents) {
  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDir())
    return false;

  base::FilePath zip_path;
  bool result =
      CreateZip(extension_dir, temp_dir.GetPath(), &zip_path) &&
      CreateCrx(zip_path, private_key, crx_path, compressed_verified_contents);
  base::DeleteFile(zip_path);
  return result;
}

bool ExtensionCreator::Run(const base::FilePath& extension_dir,
                           const base::FilePath& crx_path,
                           const base::FilePath& private_key_path,
                           int run_flags) {

  // Initialize Key Pair
  std::unique_ptr<crypto::RSAPrivateKey> key_pair;
  if (!private_key_path.value().empty())
    key_pair = ReadInputKey(private_key_path);

  if (!key_pair) {
    return false;
  }

  return CreateCrxAndPerformCleanup(extension_dir, crx_path, key_pair.get(),
                                    absl::nullopt);
}

}  // namespace extensions
