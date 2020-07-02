/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_user_model_installer/browser/user_model_file_service.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "brave/components/brave_user_model_installer/browser/regions.h"
#include "brave/components/l10n/browser/locale_helper.h"

namespace brave_user_model_installer {

namespace {

constexpr uint16_t kExpectedSchemaVersion = 1;
constexpr char kSchemaVersionPath[] = "schemaVersion";
constexpr char kModelsPath[] = "models";
constexpr char kModelsIdPath[] = "id";
constexpr char kModelsFilenamePath[] = "filename";
constexpr char kModelsVersionPath[] = "version";
constexpr char kComponentName[] = "Brave User Model Installer (%s)";
constexpr base::FilePath::CharType kManifestFile[] =
    FILE_PATH_LITERAL("models.json");

}  // namespace

UserModelFileService::UserModelFileService(
    Delegate* delegate)
    : brave_component_updater::BraveComponent(delegate) {
  // Return when testing
  // TODO(Moritz Haller): Remove and test w/ mock/fake object?
  if (!delegate) {
    return;
  }

  const std::string locale =
      brave_l10n::LocaleHelper::GetInstance()->GetLocale();
  const std::string country_code =
      brave_l10n::LocaleHelper::GetCountryCode(locale);

  const auto& region = GetRegionInfo(country_code);
  if (!region) {
    VLOG(2) << country_code << " not supported for user model installer "
        "component";
    return;
  }

  // TODO(Moritz Haller): only register when rewards/ads enabled?
  // TODO(Moritz Haller): Add buildflags for this component?
  Register(base::StringPrintf(kComponentName, country_code.c_str()),
      region->component_id, region->component_base64_public_key);
}

UserModelFileService::~UserModelFileService() = default;

std::string GetManifest(
    const base::FilePath& manifest_path) {
  std::string manifest_json;
  const bool success = base::ReadFileToString(manifest_path, &manifest_json);
  if (!success || manifest_json.empty()) {
    DVLOG(2) << "Cannot read manifest file " << manifest_path;
    return manifest_json;
  }

  return manifest_json;
}

void UserModelFileService::OnComponentReady(
    const std::string& component_id,
    const base::FilePath& install_dir,
    const std::string& manifest) {
  base::PostTaskAndReplyWithResult(FROM_HERE,
      { base::ThreadPool(), base::MayBlock() },
      base::BindOnce(&GetManifest, install_dir.Append(kManifestFile)),
      base::BindOnce(&UserModelFileService::OnGetManifest,
          weak_factory_.GetWeakPtr(), install_dir));
}

void UserModelFileService::OnGetManifest(
    const base::FilePath& install_dir,
    const std::string& manifest_json) {
  base::Optional<base::Value> manifest = base::JSONReader::Read(manifest_json);
  if (!manifest) {
    DVLOG(1) << "Failed to parse user model manifest";
    return;
  }

  if (base::Optional<int> version = manifest->FindIntPath(kSchemaVersionPath)) {
    if (kExpectedSchemaVersion != *version) {
      VLOG(1) << "User model schema version mismatch";
      return;
    }
  }

  const base::Value* models = manifest->FindListPath(kModelsPath);
  if (!models) {
    return;
  }

  for (const auto& model : models->GetList()) {
    UserModelFileInfo incoming_info;

    const std::string* id = model.FindStringPath(kModelsIdPath);
    if (id) {
      incoming_info.model_id = *id;
    }

    const base::Optional<int> version =
        model.FindIntPath(kModelsVersionPath);
    if (version) {
      incoming_info.version = *version;
    }

    const std::string* path = model.FindStringPath(kModelsFilenamePath);
    if (path) {
      incoming_info.path = install_dir.AppendASCII(*path);
    }

    auto it = user_model_files_.find(incoming_info.model_id);
    if (it != user_model_files_.end()) {
      it->second = incoming_info;
    } else {
      user_model_files_.insert({incoming_info.model_id, incoming_info});
    }

    NotifyObservers(incoming_info.model_id, incoming_info.path);
  }
}

void UserModelFileService::NotifyObservers(
    const std::string& model_id,
    const base::FilePath& model_path) {
  for (auto& observer : observers_) {
    observer.OnUserModelFilesUpdated(model_id, model_path);
  }
}

void UserModelFileService::AddObserver(
    Observer* observer) {
  DCHECK(observer);

  observers_.AddObserver(observer);
}

void UserModelFileService::RemoveObserver(
    Observer* observer) {
  DCHECK(observer);

  observers_.RemoveObserver(observer);
}

base::Optional<base::FilePath> UserModelFileService::GetPath(
    const std::string& model_id) {
  base::FilePath path;

  auto it = user_model_files_.find(model_id);
  if (it != user_model_files_.end()) {
    UserModelFileInfo info = it->second;
    return info.path;
  }

  return base::nullopt;
}

}  // namespace brave_user_model_installer
