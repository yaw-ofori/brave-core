/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/guid.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "brave/components/l10n/browser/locale_helper.h"
#include "brave/components/l10n/common/locale_util.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"
#include "bat/ads/ad_history.h"
#include "bat/ads/ads_client.h"
#include "bat/ads/internal/ad_conversions/ad_conversions.h"
#include "bat/ads/internal/ad_events/ad_notification_event_factory.h"
#include "bat/ads/internal/ads_impl.h"
#include "bat/ads/internal/bundle/bundle.h"
#include "bat/ads/internal/classification/purchase_intent_classifier/purchase_intent_signal_history.h"
#include "bat/ads/internal/eligible_ads/eligible_ads_filter_factory.h"
#include "bat/ads/internal/event_type_blur_info.h"
#include "bat/ads/internal/event_type_destroy_info.h"
#include "bat/ads/internal/event_type_focus_info.h"
#include "bat/ads/internal/event_type_load_info.h"
#include "bat/ads/internal/filters/ads_history_date_range_filter.h"
#include "bat/ads/internal/filters/ads_history_filter_factory.h"
#include "bat/ads/internal/frequency_capping/exclusion_rules/conversion_frequency_cap.h"
#include "bat/ads/internal/frequency_capping/exclusion_rules/daily_cap_frequency_cap.h"
#include "bat/ads/internal/frequency_capping/exclusion_rules/exclusion_rule.h"
#include "bat/ads/internal/frequency_capping/exclusion_rules/marked_as_inappropriate_frequency_cap.h"
#include "bat/ads/internal/frequency_capping/exclusion_rules/marked_to_no_longer_receive_frequency_cap.h"
#include "bat/ads/internal/frequency_capping/exclusion_rules/per_day_frequency_cap.h"
#include "bat/ads/internal/frequency_capping/exclusion_rules/per_hour_frequency_cap.h"
#include "bat/ads/internal/frequency_capping/exclusion_rules/subdivision_targeting_frequency_cap.h"
#include "bat/ads/internal/frequency_capping/exclusion_rules/total_max_frequency_cap.h"
#include "bat/ads/internal/frequency_capping/permission_rules/ads_per_day_frequency_cap.h"
#include "bat/ads/internal/frequency_capping/permission_rules/ads_per_hour_frequency_cap.h"
#include "bat/ads/internal/frequency_capping/permission_rules/minimum_wait_time_frequency_cap.h"
#include "bat/ads/internal/logging.h"
#include "bat/ads/internal/reports.h"
#include "bat/ads/internal/search_engine/search_providers.h"
#include "bat/ads/internal/sorts/ads_history_sort_factory.h"
#include "bat/ads/internal/static_values.h"
#include "bat/ads/internal/subdivision_targeting/subdivision_targeting.h"
#include "bat/ads/internal/time_util.h"
#include "bat/ads/internal/url_util.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#include "base/system/sys_info.h"
#endif

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

namespace {

const char kCategoryDelimiter = '-';

std::string GetDisplayUrl(const std::string& url) {
  GURL gurl(url);
  if (!gurl.is_valid())
    return std::string();

  return gurl.host();
}

}  // namespace

namespace ads {

AdsImpl::AdsImpl(AdsClient* ads_client)
    : is_foreground_(false),
      active_tab_id_(0),
      next_easter_egg_timestamp_in_seconds_(0),
      client_(std::make_unique<Client>(this)),
      bundle_(std::make_unique<Bundle>(this)),
      get_catalog_(std::make_unique<GetCatalog>(this)),
      subdivision_targeting_(std::make_unique<SubdivisionTargeting>(this)),
      ad_conversions_(std::make_unique<AdConversions>(this)),
      database_(std::make_unique<database::Initialize>(this)),
      page_classifier_(std::make_unique<classification::PageClassifier>(this)),
      purchase_intent_classifier_(std::make_unique<
          classification::PurchaseIntentClassifier>(kPurchaseIntentSignalLevel,
              kPurchaseIntentClassificationThreshold,
                  kPurchaseIntentSignalDecayTimeWindow)),
      is_initialized_(false),
      ad_notifications_(std::make_unique<AdNotifications>(this)),
      ads_client_(ads_client),
      unblinded_tokens_(std::make_unique<privacy::UnblindedTokens>(this)),
      unblinded_payment_tokens_(
          std::make_unique<privacy::UnblindedTokens>(this)),
      estimated_pending_rewards_(0.0),
      next_payment_date_in_seconds_(0),
      ad_rewards_(std::make_unique<AdRewards>(this)),
      refill_unblinded_tokens_(std::make_unique<RefillUnblindedTokens>(this,
          unblinded_tokens_.get())),
      redeem_unblinded_token_(std::make_unique<RedeemUnblindedToken>(this,
          unblinded_tokens_.get(), unblinded_payment_tokens_.get())),
      redeem_unblinded_payment_tokens_(std::make_unique<
          RedeemUnblindedPaymentTokens>(this, unblinded_payment_tokens_.get())),
      state_has_loaded_(false)) {
  set_ads_client_for_logging(ads_client_);

  redeem_unblinded_token_->set_delegate(this);
  redeem_unblinded_payment_tokens_->set_delegate(this);
  refill_unblinded_tokens_->set_delegate(this);
}

AdsImpl::~AdsImpl() = default;

AdsClient* AdsImpl::get_ads_client() const {
  return ads_client_;
}

Client* AdsImpl::get_client() const {
  return client_.get();
}

AdNotifications* AdsImpl::get_ad_notifications() const {
  return ad_notifications_.get();
}

SubdivisionTargeting* AdsImpl::get_subdivision_targeting() const {
  return subdivision_targeting_.get();
}

classification::PageClassifier* AdsImpl::get_page_classifier() const {
  return page_classifier_.get();
}

privacy::UnblindedTokens* AdsImpl::get_unblinded_tokens() const {
  return unblinded_tokens_.get();
}

privacy::UnblindedTokens* AdsImpl::get_unblinded_payment_tokens() const {
  return unblinded_payment_tokens_.get();
}

Bundle* AdsImpl::get_bundle() const {
  return bundle_.get();
}

AdConversions* AdsImpl::get_ad_conversions() const {
  return ad_conversions_.get();
}

void AdsImpl::Initialize(
    InitializeCallback callback) {
  BLOG(1, "Initializing ads");

  initialize_callback_ = callback;

  if (IsInitialized()) {
    BLOG(1, "Already initialized ads");

    initialize_callback_(FAILED);
    return;
  }

  auto initialize_step_2_callback =
      std::bind(&AdsImpl::InitializeStep2, this, _1);
  database_->CreateOrOpen(initialize_step_2_callback);
}

void AdsImpl::InitializeStep2(
    const Result result) {
  if (result != SUCCESS) {
    BLOG(0, "Failed to initialize database: " << database_->get_last_message());
    initialize_callback_(FAILED);
    return;
  }

  const auto callback = std::bind(&AdsImpl::InitializeStep3, this, _1);
  client_->Initialize(callback);
}

void AdsImpl::InitializeStep3(
    const Result result) {
  if (result != SUCCESS) {
    initialize_callback_(FAILED);
    return;
  }

  const auto callback = std::bind(&AdsImpl::InitializeStep4, this, _1);
  ad_notifications_->Initialize(callback);
}

void AdsImpl::InitializeStep4(
    const Result result) {
  if (result != SUCCESS) {
    initialize_callback_(FAILED);
    return;
  }

  const auto callback = std::bind(&AdsImpl::InitializeStep5, this, _1);
  ad_conversions_->Initialize(callback);
}

void AdsImpl::InitializeStep5(
    const Result result) {
  if (result != SUCCESS) {
    initialize_callback_(FAILED);
    return;
  }

  auto user_model_languages = ads_client_->GetUserModelLanguages();
  client_->SetUserModelLanguages(user_model_languages);

  const std::string locale =
      brave_l10n::LocaleHelper::GetInstance()->GetLocale();

  ChangeLocale(locale);
}

void AdsImpl::InitializeStep6(
    const Result result) {
  if (result != SUCCESS) {
    initialize_callback_(FAILED);
    return;
  }

  is_initialized_ = true;

  BLOG(1, "Successfully initialized ads");

  is_foreground_ = ads_client_->IsForeground();

  ads_client_->SetIdleThreshold(kIdleThresholdInSeconds);

  initialize_callback_(SUCCESS);

  ad_conversions_->StartTimerIfReady();

  MaybeServeAdNotification(false);

#if defined(OS_ANDROID)
    // Ad notifications do not sustain a reboot or update, so we should remove
    // orphaned ad notifications
    RemoveAllAdNotificationsAfterReboot();
    RemoveAllAdNotificationsAfterUpdate();
#endif

  client_->UpdateAdUUID();

  if (IsMobile()) {
    if (client_->GetNextCheckServeAdNotificationTimestampInSeconds() == 0) {
      StartDeliveringAdNotificationsAfterSeconds(
          2 * base::Time::kSecondsPerMinute);
    } else {
      StartDeliveringAdNotifications();
    }
  }

  get_catalog_->DownloadCatalog();
}

#if defined(OS_ANDROID)
void AdsImpl::RemoveAllAdNotificationsAfterReboot() {
  auto ads_shown_history = client_->GetAdsHistory();
  if (!ads_shown_history.empty()) {
    uint64_t ad_shown_timestamp =
        ads_shown_history.front().timestamp_in_seconds;
    uint64_t boot_timestamp =
        static_cast<uint64_t>(base::Time::Now().ToDoubleT() -
            static_cast<uint64_t>(base::SysInfo::Uptime().InSeconds()));
    if (ad_shown_timestamp <= boot_timestamp) {
      ad_notifications_->RemoveAll(false);
    }
  }
}

void AdsImpl::RemoveAllAdNotificationsAfterUpdate() {
  // Ad notifications do not sustain app update, so remove all ad notifications
  std::string current_version_code(
      base::android::BuildInfo::GetInstance()->package_version_code());
  std::string last_version_code = client_->GetVersionCode();
  if (last_version_code != current_version_code) {
    client_->SetVersionCode(current_version_code);
    ad_notifications_->RemoveAll(false);
  }
}
#endif

bool AdsImpl::IsInitialized() {
  if (!is_initialized_ || !ads_client_->IsEnabled()) {
    return false;
  }

  if (page_classifier_->ShouldClassifyPages() &&
      !page_classifier_->IsInitialized()) {
    return false;
  }

  return true;
}

void AdsImpl::Shutdown(
    ShutdownCallback callback) {
  if (!is_initialized_) {
    BLOG(0, "Shutdown failed as not initialized");

    callback(FAILED);
    return;
  }

  ad_notifications_->RemoveAll(true);

  callback(SUCCESS);
}

void AdsImpl::LoadUserModel() {
  auto language = client_->GetUserModelLanguage();

  const auto callback = std::bind(&AdsImpl::OnUserModelLoaded, this, _1, _2);
  ads_client_->LoadUserModelForLanguage(language, callback);
}

void AdsImpl::OnUserModelLoaded(
    const Result result,
    const std::string& json) {
  auto language = client_->GetUserModelLanguage();

  if (result != SUCCESS) {
    BLOG(0, "Failed to load user model for " << language << " language");
    return;
  }

  BLOG(3, "Successfully loaded user model for " << language << " language");

  if (!page_classifier_->Initialize(json)) {
    BLOG(0, "Failed to initialize page classification user model for "
        << language << " language");
    return;
  }

  BLOG(1, "Successfully initialized page classification user model for "
      << language << " language");

  if (!IsInitialized()) {
    InitializeStep6(SUCCESS);
  }
}

bool AdsImpl::IsMobile() const {
  ClientInfo client_info;
  ads_client_->GetClientInfo(&client_info);

  if (client_info.platform != ANDROID_OS && client_info.platform != IOS) {
    return false;
  }

  return true;
}

bool AdsImpl::GetAdNotification(
    const std::string& uuid,
    AdNotificationInfo* notification) {
  return ad_notifications_->Get(uuid, notification);
}

void AdsImpl::OnForeground() {
  is_foreground_ = true;

  const Reports reports(this);
  const std::string report = reports.GenerateForegroundEventReport();
  BLOG(3, "Event log: " << report);

  if (IsMobile() && !ads_client_->CanShowBackgroundNotifications()) {
    StartDeliveringAdNotifications();
  }
}

void AdsImpl::OnBackground() {
  is_foreground_ = false;

  const Reports reports(this);
  const std::string report = reports.GenerateBackgroundEventReport();
  BLOG(3, "Event log: " << report);

  if (IsMobile() && !ads_client_->CanShowBackgroundNotifications()) {
    deliver_ad_notification_timer_.Stop();
  }
}

bool AdsImpl::IsForeground() const {
  return is_foreground_;
}

void AdsImpl::OnIdle() {
  BLOG(1, "Browser state changed to idle");
}

void AdsImpl::OnUnIdle() {
  if (!IsInitialized()) {
    BLOG(0, "OnUnIdle failed as not initialized");
    return;
  }

  BLOG(1, "Browser state changed to unidle");

  if (IsMobile()) {
    return;
  }

  MaybeServeAdNotification(true);
}

void AdsImpl::OnMediaPlaying(
    const int32_t tab_id) {
  const auto iter = media_playing_.find(tab_id);
  if (iter != media_playing_.end()) {
    // Media is already playing for this tab
    return;
  }

  BLOG(2, "Started playing media for tab id " << tab_id);

  media_playing_.insert(tab_id);
}

void AdsImpl::OnMediaStopped(
    const int32_t tab_id) {
  const auto iter = media_playing_.find(tab_id);
  if (iter == media_playing_.end()) {
    // Media is not playing for this tab
    return;
  }

  BLOG(2, "Stopped playing media for tab id " << tab_id);

  media_playing_.erase(iter);
}

bool AdsImpl::IsMediaPlaying() const {
  const auto iter = media_playing_.find(active_tab_id_);
  if (iter == media_playing_.end()) {
    // Media is not playing in the active tab
    return false;
  }

  return true;
}

void AdsImpl::OnAdNotificationEvent(
    const std::string& uuid,
    const AdNotificationEventType event_type) {
  DCHECK(!uuid.empty());

  AdNotificationInfo info;
  if (!ad_notifications_->Get(uuid, &info)) {
    BLOG(1, "Failed to trigger ad event as an ad notification was not found "
        "for uuid " << uuid);

    return;
  }

  const auto ad_event = AdEventFactory::Build(this, event_type);
  ad_event->Trigger(info);
}

bool AdsImpl::ShouldNotDisturb() const {
  if (!IsAndroid()) {
    return false;
  }

  if (IsForeground()) {
    return false;
  }

  auto now = base::Time::Now();
  base::Time::Exploded now_exploded;
  now.LocalExplode(&now_exploded);

  if (now_exploded.hour >= kDoNotDisturbToHour &&
      now_exploded.hour <= kDoNotDisturbFromHour) {
    return false;
  }

  return true;
}

bool AdsImpl::IsAndroid() const {
  ClientInfo client_info;
  ads_client_->GetClientInfo(&client_info);

  if (client_info.platform != ANDROID_OS) {
    return false;
  }

  return true;
}

void AdsImpl::OnTabUpdated(
    const int32_t tab_id,
    const std::string& url,
    const bool is_active,
    const bool is_incognito) {
  if (is_incognito) {
    return;
  }

  if (is_active) {
    BLOG(2, "Tab id " << tab_id << " is visible");

    active_tab_id_ = tab_id;
    previous_tab_url_ = active_tab_url_;
    active_tab_url_ = url;

    const Reports reports(this);
    FocusInfo focus_info;
    focus_info.tab_id = tab_id;
    const std::string report = reports.GenerateFocusEventReport(focus_info);
    BLOG(3, "Event log: " << report);
  } else {
    BLOG(3, "Tab id " << tab_id << " is occluded");

    const Reports reports(this);
    BlurInfo blur_info;
    blur_info.tab_id = tab_id;
    const std::string report = reports.GenerateBlurEventReport(blur_info);
    BLOG(3, "Event log: " << report);
  }
}

void AdsImpl::OnTabClosed(
    const int32_t tab_id) {
  BLOG(2, "Tab id " << tab_id << " was closed");

  OnMediaStopped(tab_id);

  const Reports reports(this);
  DestroyInfo destroy_info;
  destroy_info.tab_id = tab_id;
  const std::string report = reports.GenerateDestroyEventReport(destroy_info);
  BLOG(3, "Event log: " << report);
}

void AdsImpl::RemoveAllHistory(
    RemoveAllHistoryCallback callback) {
  client_->RemoveAllHistory();

  callback(SUCCESS);
}

AdsHistory AdsImpl::GetAdsHistory(
    const AdsHistory::FilterType filter_type,
    const AdsHistory::SortType sort_type,
    const uint64_t from_timestamp,
    const uint64_t to_timestamp) {
  auto history = client_->GetAdsHistory();

  const auto date_range_filter = std::make_unique<AdsHistoryDateRangeFilter>();
  DCHECK(date_range_filter);
  if (date_range_filter) {
    history = date_range_filter->Apply(history, from_timestamp, to_timestamp);
  }

  const auto filter = AdsHistoryFilterFactory::Build(filter_type);
  DCHECK(filter);
  if (filter) {
    history = filter->Apply(history);
  }

  const auto sort = AdsHistorySortFactory::Build(sort_type);
  DCHECK(sort);
  if (sort) {
    history = sort->Apply(history);
  }

  AdsHistory ads_history;
  for (const auto& entry : history) {
    ads_history.entries.push_back(entry);
  }

  return ads_history;
}

AdContent::LikeAction AdsImpl::ToggleAdThumbUp(
    const std::string& creative_instance_id,
    const std::string& creative_set_id,
    const AdContent::LikeAction& action) {
  auto like_action =
      client_->ToggleAdThumbUp(creative_instance_id, creative_set_id, action);
  if (like_action == AdContent::LikeAction::kThumbsUp) {
    ConfirmAction(creative_instance_id, creative_set_id,
        ConfirmationType::kUpvoted);
  }

  return like_action;
}

AdContent::LikeAction AdsImpl::ToggleAdThumbDown(
    const std::string& creative_instance_id,
    const std::string& creative_set_id,
    const AdContent::LikeAction& action) {
  auto like_action =
      client_->ToggleAdThumbDown(creative_instance_id, creative_set_id, action);
  if (like_action == AdContent::LikeAction::kThumbsDown) {
    ConfirmAction(creative_instance_id, creative_set_id,
        ConfirmationType::kDownvoted);
  }

  return like_action;
}

CategoryContent::OptAction AdsImpl::ToggleAdOptInAction(
    const std::string& category,
    const CategoryContent::OptAction& action) {
  return client_->ToggleAdOptInAction(category, action);
}

CategoryContent::OptAction AdsImpl::ToggleAdOptOutAction(
    const std::string& category,
    const CategoryContent::OptAction& action) {
  return client_->ToggleAdOptOutAction(category, action);
}

bool AdsImpl::ToggleSaveAd(
    const std::string& creative_instance_id,
    const std::string& creative_set_id,
    const bool saved) {
  return client_->ToggleSaveAd(creative_instance_id, creative_set_id, saved);
}

bool AdsImpl::ToggleFlagAd(
    const std::string& creative_instance_id,
    const std::string& creative_set_id,
    const bool flagged) {
  auto flag_ad =
      client_->ToggleFlagAd(creative_instance_id, creative_set_id, flagged);
  if (flag_ad) {
    ConfirmAction(creative_instance_id, creative_set_id,
        ConfirmationType::kFlagged);
  }

  return flag_ad;
}

void AdsImpl::ChangeLocale(
    const std::string& locale) {
  subdivision_targeting_->MaybeFetch(locale);

  const std::string language_code = brave_l10n::GetLanguageCode(locale);
  client_->SetUserModelLanguage(language_code);

  if (!page_classifier_->ShouldClassifyPages()) {
    client_->SetUserModelLanguage(language_code);

    InitializeStep6(SUCCESS);
    return;
  }

  LoadUserModel();
}

void AdsImpl::OnAdsSubdivisionTargetingCodeHasChanged() {
  const std::string locale =
      brave_l10n::LocaleHelper::GetInstance()->GetLocale();

  subdivision_targeting_->MaybeFetch(locale);
}

void AdsImpl::OnPageLoaded(
    const std::string& url,
    const std::string& content) {
  DCHECK(!url.empty());

  if (!IsInitialized()) {
    BLOG(0, "Failed to classify page as not initialized");
    return;
  }

  const bool is_supported_url = GURL(url).is_valid();

  if (is_supported_url) {
    ad_conversions_->Check(url);
  }

  ExtractPurchaseIntentSignal(url);

  if (SameSite(url, last_shown_ad_notification_.target_url)) {
    BLOG(1, "Visited URL matches the last shown ad notification");

    if (last_sustained_ad_notification_.creative_instance_id !=
        last_shown_ad_notification_.creative_instance_id) {
      last_sustained_ad_notification_ = AdNotificationInfo();
    }

    if (!SameSite(url, last_sustained_ad_notification_.target_url)) {
      last_sustained_ad_notification_ = last_shown_ad_notification_;

      StartSustainingAdNotificationInteraction();
    } else {
      if (sustain_ad_notification_interaction_timer_.IsRunning()) {
        BLOG(1, "Already sustaining ad for visited URL");
      } else {
        BLOG(1, "Already sustained ad for visited URL");
      }
    }

    return;
  }

  if (!last_shown_ad_notification_.target_url.empty()) {
    BLOG(1, "Visited URL does not match the last shown ad notification");
  }

  if (!is_supported_url) {
    BLOG(1, "Page not classified as visited URL is not supported");
    return;
  }

  if (SearchProviders::IsSearchEngine(url)) {
    BLOG(1, "Page not classified as visited URL is a search engine");
    return;
  }

  MaybeClassifyPage(url, content);
}

void AdsImpl::ExtractPurchaseIntentSignal(
    const std::string& url) {
  if (!page_classifier_->ShouldClassifyPages()) {
    return;
  }

  if (!SearchProviders::IsSearchEngine(url) &&
      SameSite(url, previous_tab_url_)) {
    return;
  }

  classification::PurchaseIntentSignalInfo purchase_intent_signal =
      purchase_intent_classifier_->ExtractIntentSignal(url);

  if (purchase_intent_signal.segments.empty() &&
      purchase_intent_signal.timestamp_in_seconds == 0) {
    return;
  }

  BLOG(1, "Extracting purchase intent signal from visited URL");

  GeneratePurchaseIntentSignalHistoryEntry(purchase_intent_signal);
}

void AdsImpl::GeneratePurchaseIntentSignalHistoryEntry(
    const classification::PurchaseIntentSignalInfo& purchase_intent_signal) {
  for (const auto& segment : purchase_intent_signal.segments) {
    PurchaseIntentSignalHistory history;
    history.timestamp_in_seconds = purchase_intent_signal.timestamp_in_seconds;
    history.weight = purchase_intent_signal.weight;
    client_->AppendToPurchaseIntentSignalHistoryForSegment(segment, history);
  }
}

void AdsImpl::MaybeClassifyPage(
    const std::string& url,
    const std::string& content) {
  std::string page_classification;

  if (page_classifier_->ShouldClassifyPages()) {
    page_classification = page_classifier_->ClassifyPage(url, content);
    if (page_classification.empty()) {
      BLOG(1, "Page not classified as not enough content");
    } else {
      const classification::CategoryList winning_categories =
          page_classifier_->GetWinningCategories();

      BLOG(1, "Classified page as " << page_classification << ". Winning "
          "page classification over time is " << winning_categories.front());
    }
  } else {
    page_classification = kUntargetedPageClassification;
  }

  LoadInfo load_info;
  load_info.tab_id = active_tab_id_;
  load_info.tab_url = active_tab_url_;
  load_info.tab_classification = page_classification;

  const Reports reports(this);
  const std::string report = reports.GenerateLoadEventReport(load_info);
  BLOG(3, "Event log: " << report);
}

classification::PurchaseIntentWinningCategoryList
AdsImpl::GetWinningPurchaseIntentCategories() {
  classification::PurchaseIntentWinningCategoryList winning_categories;

  PurchaseIntentSignalSegmentHistoryMap purchase_intent_signal_history =
      client_->GetPurchaseIntentSignalHistory();
  if (purchase_intent_signal_history.empty()) {
    return winning_categories;
  }

  winning_categories = purchase_intent_classifier_->GetWinningCategories(
      purchase_intent_signal_history, kPurchaseIntentMaxSegments);

  return winning_categories;
}

void AdsImpl::ServeAdNotificationIfReady() {
  if (!IsInitialized()) {
    FailedToServeAdNotification("Not initialized");
    return;
  }

  if (!bundle_->IsReady()) {
    FailedToServeAdNotification("Bundle not ready");
    return;
  }

  if (!IsAndroid() && !IsForeground()) {
    FailedToServeAdNotification("Not in foreground");
    return;
  }

  if (IsMediaPlaying()) {
    FailedToServeAdNotification("Media playing in browser");
    return;
  }

  if (ShouldNotDisturb()) {
    FailedToServeAdNotification("Should not disturb");
    return;
  }

  if (!IsAllowedToServeAdNotifications()) {
    FailedToServeAdNotification("Not allowed based on history");
    return;
  }

  classification::CategoryList categories = GetCategoriesToServeAd();
  ServeAdNotificationFromCategories(categories);
}

classification::CategoryList AdsImpl::GetCategoriesToServeAd() {
  classification::CategoryList categories =
      page_classifier_->GetWinningCategories();

  classification::PurchaseIntentWinningCategoryList purchase_intent_categories =
      GetWinningPurchaseIntentCategories();
  categories.insert(categories.end(),
      purchase_intent_categories.begin(), purchase_intent_categories.end());

  return categories;
}

void AdsImpl::ServeAdNotificationFromCategories(
    const classification::CategoryList& categories) {
  std::string catalog_id = bundle_->GetCatalogId();
  if (catalog_id.empty()) {
    FailedToServeAdNotification("No ad catalog");
    return;
  }

  if (categories.empty()) {
    BLOG(1, "No categories to serve targeted ads");
    ServeUntargetedAdNotification();
    return;
  }

  BLOG(1, "Serving ad from categories:");
  for (const auto& category : categories) {
    BLOG(1, "  " << category);
  }

  const auto callback = std::bind(&AdsImpl::OnServeAdNotificationFromCategories,
      this, _1, _2, _3);

  database::table::CreativeAdNotifications database_table(this);
  database_table.GetCreativeAdNotifications(categories, callback);
}

void AdsImpl::OnServeAdNotificationFromCategories(
    const Result result,
    const classification::CategoryList& categories,
    const CreativeAdNotificationList& ads) {
  auto eligible_ads = GetEligibleAds(ads);
  if (!eligible_ads.empty()) {
    ServeAdNotificationWithPacing(eligible_ads);
    return;
  }

  BLOG(1, "No eligible ads found in categories:");
  for (const auto& category : categories) {
    BLOG(1, "  " << category);
  }

  // TODO(https://github.com/brave/brave-browser/issues/8486): Brave Ads
  // Purchase Intent segments should not fall back to parent segments
  if (ServeAdNotificationFromParentCategories(categories)) {
    return;
  }

  ServeUntargetedAdNotification();
}

bool AdsImpl::ServeAdNotificationFromParentCategories(
    const classification::CategoryList& categories) {
  classification::CategoryList parent_categories;
  for (const auto& category : categories) {
    auto pos = category.find_last_of(kCategoryDelimiter);
    if (pos == std::string::npos) {
      return false;
    }

    std::string parent_category = category.substr(0, pos);

    if (std::find(parent_categories.begin(), parent_categories.end(),
        parent_category) != parent_categories.end()) {
      continue;
    }

    parent_categories.push_back(parent_category);
  }

  BLOG(1, "Serving ad from parent categories:");
  for (const auto& parent_category : parent_categories) {
    BLOG(1, "  " << parent_category);
  }

  const auto callback = std::bind(&AdsImpl::OnServeAdNotificationFromCategories,
      this, _1, _2, _3);

  database::table::CreativeAdNotifications database_table(this);
  database_table.GetCreativeAdNotifications(parent_categories, callback);

  return true;
}

void AdsImpl::ServeUntargetedAdNotification() {
  BLOG(1, "Serving ad notification from untargeted category");

  std::vector<std::string> categories = {
    kUntargetedPageClassification
  };

  const auto callback = std::bind(&AdsImpl::OnServeUntargetedAdNotification,
      this, _1, _2, _3);

  database::table::CreativeAdNotifications database_table(this);
  database_table.GetCreativeAdNotifications(categories, callback);
}

void AdsImpl::OnServeUntargetedAdNotification(
    const Result result,
    const classification::CategoryList& categories,
    const CreativeAdNotificationList& ads) {
  auto eligible_ads = GetEligibleAds(ads);
  if (eligible_ads.empty()) {
    FailedToServeAdNotification("No eligible ads found");
    return;
  }

  ServeAdNotificationWithPacing(eligible_ads);
}

void AdsImpl::ServeAdNotificationWithPacing(
    const CreativeAdNotificationList& ads) {
  const auto filter = EligibleAdsFilterFactory::Build(
      EligibleAdsFilter::Type::kPriority);
  DCHECK(filter);

  const CreativeAdNotificationList eligible_ads = filter->Apply(ads);
  if (eligible_ads.empty()) {
    FailedToServeAdNotification("No eligible ads found");
    return;
  }

  BLOG(1, "Found " << eligible_ads.size() << " eligible ads");

  const int rand = base::RandInt(0, eligible_ads.size() - 1);
  const CreativeAdNotificationInfo ad = eligible_ads.at(rand);

  if (ad.priority == 0) {
    FailedToServeAdNotification("Pacing ad delivery [0]");
    return;
  }

  const int rand_priority = base::RandInt(1, ad.priority);
  if (rand_priority != 1) {
    const std::string message = base::StringPrintf("Pacing ad delivery "
        "[Roll(%d):%d]", ad.priority, rand_priority);

    FailedToServeAdNotification(message);
    return;
  }

  ShowAdNotification(ad);

  SuccessfullyServedAd();
}

void AdsImpl::SuccessfullyServedAd() {
  if (IsMobile()) {
    StartDeliveringAdNotificationsAfterSeconds(
        base::Time::kSecondsPerHour / ads_client_->GetAdsPerHour());
  }
}

void AdsImpl::FailedToServeAdNotification(
    const std::string& reason) {
  BLOG(1, "Ad notification not shown: " << reason);

  if (IsMobile()) {
    StartDeliveringAdNotificationsAfterSeconds(
        2 * base::Time::kSecondsPerMinute);
  }
}

std::vector<std::unique_ptr<ExclusionRule>>
    AdsImpl::CreateExclusionRules() const {
  std::vector<std::unique_ptr<ExclusionRule>> exclusion_rules;

  std::unique_ptr<ExclusionRule> daily_cap_frequency_cap =
      std::make_unique<DailyCapFrequencyCap>(this);
  exclusion_rules.push_back(std::move(daily_cap_frequency_cap));

  std::unique_ptr<ExclusionRule> per_day_frequency_cap =
      std::make_unique<PerDayFrequencyCap>(this);
  exclusion_rules.push_back(std::move(per_day_frequency_cap));

  std::unique_ptr<ExclusionRule> per_hour_frequency_cap =
      std::make_unique<PerHourFrequencyCap>(this);
  exclusion_rules.push_back(std::move(per_hour_frequency_cap));

  std::unique_ptr<ExclusionRule> total_max_frequency_cap =
      std::make_unique<TotalMaxFrequencyCap>(this);
  exclusion_rules.push_back(std::move(total_max_frequency_cap));

  std::unique_ptr<ExclusionRule> conversion_frequency_cap =
      std::make_unique<ConversionFrequencyCap>(this);
  exclusion_rules.push_back(std::move(conversion_frequency_cap));

  std::unique_ptr<ExclusionRule> subdivision_targeting_frequency_cap =
      std::make_unique<SubdivisionTargetingFrequencyCap>(this);
  exclusion_rules.push_back(std::move(subdivision_targeting_frequency_cap));

  std::unique_ptr<ExclusionRule> marked_to_no_longer_recieve_frequency_cap =
      std::make_unique<MarkedToNoLongerReceiveFrequencyCap>(this);
  exclusion_rules.push_back(std::move(
      marked_to_no_longer_recieve_frequency_cap));

  std::unique_ptr<ExclusionRule> marked_as_inappropriate_frequency_cap =
      std::make_unique<MarkedAsInappropriateFrequencyCap>(this);
  exclusion_rules.push_back(std::move(marked_as_inappropriate_frequency_cap));

  return exclusion_rules;
}

CreativeAdNotificationList AdsImpl::GetEligibleAds(
    const CreativeAdNotificationList& ads) {
  CreativeAdNotificationList eligible_ads;

  const auto exclusion_rules = CreateExclusionRules();

  std::set<std::string> exclusions;

  auto unseen_ads = GetUnseenAdsAndRoundRobinIfNeeded(ads);
  for (const auto& ad : unseen_ads) {
    bool should_exclude = false;

    for (const auto& exclusion_rule : exclusion_rules) {
      if (!exclusion_rule->ShouldExclude(ad)) {
        continue;
      }

      exclusions.insert(exclusion_rule->get_last_message());

      should_exclude = true;
    }

    if (should_exclude) {
      continue;
    }

    eligible_ads.push_back(ad);
  }

  for (const auto& exclusion : exclusions) {
    BLOG(2, exclusion);
  }

  return eligible_ads;
}

CreativeAdNotificationList AdsImpl::GetUnseenAdsAndRoundRobinIfNeeded(
    const CreativeAdNotificationList& ads) const {
  if (ads.empty()) {
    return ads;
  }

  CreativeAdNotificationList ads_for_unseen_advertisers =
      GetAdsForUnseenAdvertisers(ads);
  if (ads_for_unseen_advertisers.empty()) {
    BLOG(1, "All advertisers have been shown, so round robin");

    const bool should_not_show_last_advertiser =
        client_->GetSeenAdvertisers().size() > 1 ? true : false;

    client_->ResetSeenAdvertisers(ads);

    ads_for_unseen_advertisers = GetAdsForUnseenAdvertisers(ads);

    if (should_not_show_last_advertiser) {
      const auto it = std::remove_if(ads_for_unseen_advertisers.begin(),
          ads_for_unseen_advertisers.end(),
              [&](CreativeAdNotificationInfo& info) {
        return info.advertiser_id ==
            last_shown_creative_ad_notification_.advertiser_id;
      });

      ads_for_unseen_advertisers.erase(it, ads_for_unseen_advertisers.end());
    }
  }

  CreativeAdNotificationList unseen_ads =
      GetUnseenAds(ads_for_unseen_advertisers);
  if (unseen_ads.empty()) {
    BLOG(1, "All ads have been shown, so round robin");

    const bool should_not_show_last_ad =
        client_->GetSeenAdNotifications().size() > 1 ? true : false;

    client_->ResetSeenAdNotifications(ads);

    unseen_ads = GetUnseenAds(ads);

    if (should_not_show_last_ad) {
      const auto it = std::remove_if(ads_for_unseen_advertisers.begin(),
          ads_for_unseen_advertisers.end(),
              [&](CreativeAdNotificationInfo& info) {
        return info.creative_instance_id ==
            last_shown_creative_ad_notification_.creative_instance_id;
      });

      ads_for_unseen_advertisers.erase(it, ads_for_unseen_advertisers.end());
    }
  }

  return unseen_ads;
}

CreativeAdNotificationList AdsImpl::GetUnseenAds(
    const CreativeAdNotificationList& ads) const {
  auto unseen_ads = ads;
  const auto seen_ads = client_->GetSeenAdNotifications();
  const auto seen_advertisers = client_->GetSeenAdvertisers();

  const auto it = std::remove_if(unseen_ads.begin(), unseen_ads.end(),
      [&](CreativeAdNotificationInfo& info) {
    return seen_ads.find(info.creative_instance_id) != seen_ads.end() &&
        seen_ads.find(info.advertiser_id) != seen_advertisers.end();
  });

  unseen_ads.erase(it, unseen_ads.end());

  return unseen_ads;
}

CreativeAdNotificationList AdsImpl::GetAdsForUnseenAdvertisers(
    const CreativeAdNotificationList& ads) const {
  auto unseen_ads = ads;
  const auto seen_ads = client_->GetSeenAdvertisers();

  const auto it = std::remove_if(unseen_ads.begin(), unseen_ads.end(),
      [&seen_ads](CreativeAdNotificationInfo& info) {
    return seen_ads.find(info.advertiser_id) != seen_ads.end();
  });

  unseen_ads.erase(it, unseen_ads.end());

  return unseen_ads;
}

bool AdsImpl::IsAdNotificationValid(  // TODO(tmancey)
    const CreativeAdNotificationInfo& info) {
  if (info.title.empty() ||
      info.body.empty() ||
      info.target_url.empty()) {
    BLOG(1, "Ad notification not shown: Incomplete ad information:\n"
        << "  creativeInstanceId: " << info.creative_instance_id << "\n"
        << "  creativeSetId: " << info.creative_set_id << "\n"
        << "  campaignId: " << info.campaign_id << "\n"
        << "  title: " << info.title << "\n"
        << "  body: " << info.body << "\n"
        << "  targetUrl: " << info.target_url);

    return false;
  }

  return true;
}

bool AdsImpl::ShowAdNotification(
    const CreativeAdNotificationInfo& info) {
  if (!IsAdNotificationValid(info)) {
    return false;
  }

  auto now_in_seconds = static_cast<uint64_t>(base::Time::Now().ToDoubleT());

  client_->AppendTimestampToCreativeSetHistory(
      info.creative_set_id, now_in_seconds);
  client_->AppendTimestampToCampaignHistory(
      info.campaign_id, now_in_seconds);

  client_->UpdateSeenAdNotification(info.creative_instance_id, 1);
  client_->UpdateSeenAdvertiser(info.advertiser_id, 1);

  last_shown_creative_ad_notification_ = info;

  auto ad_notification = std::make_unique<AdNotificationInfo>();
  ad_notification->uuid = base::GenerateGUID();
  ad_notification->parent_uuid = base::GenerateGUID();
  ad_notification->creative_instance_id = info.creative_instance_id;
  ad_notification->creative_set_id = info.creative_set_id;
  ad_notification->category = info.category;
  ad_notification->title = info.title;
  ad_notification->body = info.body;
  ad_notification->target_url = info.target_url;
  ad_notification->geo_target = info.geo_targets.at(0);

  BLOG(1, "Ad notification shown:\n"
      << "  uuid: " << ad_notification->uuid << "\n"
      << "  parentUuid: " << ad_notification->parent_uuid << "\n"
      << "  creativeInstanceId: "
          << ad_notification->creative_instance_id << "\n"
      << "  creativeSetId: " << ad_notification->creative_set_id << "\n"
      << "  category: " << ad_notification->category << "\n"
      << "  title: " << ad_notification->title << "\n"
      << "  body: " << ad_notification->body << "\n"
      << "  targetUrl: " << ad_notification->target_url);

  ad_notifications_->PushBack(*ad_notification);

  if (kMaximumAdNotifications > 0 &&
      ad_notifications_->Count() > kMaximumAdNotifications) {
    ad_notifications_->PopFront(true);
  }

  return true;
}

std::vector<std::unique_ptr<PermissionRule>>
    AdsImpl::CreatePermissionRules() const {
  std::vector<std::unique_ptr<PermissionRule>> permission_rules;

  std::unique_ptr<PermissionRule> ads_per_hour_frequency_cap =
      std::make_unique<AdsPerHourFrequencyCap>(this);
  permission_rules.push_back(std::move(ads_per_hour_frequency_cap));

  std::unique_ptr<PermissionRule> minimum_wait_time_frequency_cap =
      std::make_unique<MinimumWaitTimeFrequencyCap>(this);
  permission_rules.push_back(std::move(minimum_wait_time_frequency_cap));

  std::unique_ptr<PermissionRule> ads_per_day_frequency_cap =
      std::make_unique<AdsPerDayFrequencyCap>(this);
  permission_rules.push_back(std::move(ads_per_day_frequency_cap));

  return permission_rules;
}

bool AdsImpl::IsAllowedToServeAdNotifications() {
  const auto permission_rules = CreatePermissionRules();

  bool is_allowed = true;

  for (const auto& permission_rule : permission_rules) {
    if (permission_rule->IsAllowed()) {
      continue;
    }

    BLOG(2, permission_rule->get_last_message());

    is_allowed = false;
  }

  return is_allowed;
}

void AdsImpl::StartDeliveringAdNotifications() {
  auto now_in_seconds = static_cast<uint64_t>(base::Time::Now().ToDoubleT());
  auto next_check_serve_ad_timestamp_in_seconds =
      client_->GetNextCheckServeAdNotificationTimestampInSeconds();

  uint64_t delay;
  if (now_in_seconds >= next_check_serve_ad_timestamp_in_seconds) {
    // Browser was launched after the next check to serve an ad
    delay = 1 * base::Time::kSecondsPerMinute;
  } else {
    delay = next_check_serve_ad_timestamp_in_seconds - now_in_seconds;
  }

  const base::Time time = deliver_ad_notification_timer_.Start(delay,
      base::BindOnce(&AdsImpl::DeliverAdNotification, base::Unretained(this)));

  BLOG(1, "Attempt to deliver next ad notification "
      << FriendlyDateAndTime(time));
}

void AdsImpl::StartDeliveringAdNotificationsAfterSeconds(
    const uint64_t seconds) {
  auto timestamp_in_seconds =
      static_cast<uint64_t>(base::Time::Now().ToDoubleT() + seconds);
  client_->SetNextCheckServeAdNotificationTimestampInSeconds(
      timestamp_in_seconds);

  StartDeliveringAdNotifications();
}

void AdsImpl::DeliverAdNotification() {
  MaybeServeAdNotification(true);
}

bool AdsImpl::IsCatalogOlderThanOneDay() {
  auto catalog_last_updated_timestamp_in_seconds =
    bundle_->GetCatalogLastUpdatedTimestampInSeconds();

  auto now_in_seconds = static_cast<uint64_t>(base::Time::Now().ToDoubleT());

  if (catalog_last_updated_timestamp_in_seconds != 0 &&
      now_in_seconds > catalog_last_updated_timestamp_in_seconds
          + (base::Time::kSecondsPerHour * base::Time::kHoursPerDay)) {
    return true;
  }

  return false;
}

void AdsImpl::MaybeServeAdNotification(
    const bool should_serve) {
  auto ok = ads_client_->ShouldShowNotifications();

  auto previous = client_->GetAvailable();

  if (ok != previous) {
    client_->SetAvailable(ok);
  }

  if (!should_serve || ok != previous) {
    const Reports reports(this);
    const std::string report = reports.GenerateSettingsEventReport();
    BLOG(3, "Event log: " << report);
  }

  if (!should_serve) {
    return;
  }

  if (!ok) {
    FailedToServeAdNotification("Notifications not allowed");
    return;
  }

  if (!ads_client_->IsNetworkConnectionAvailable()) {
    FailedToServeAdNotification("Network connection not available");
    return;
  }

  if (IsCatalogOlderThanOneDay()) {
    FailedToServeAdNotification("Catalog older than one day");
    return;
  }

  ServeAdNotificationIfReady();
}

const AdNotificationInfo& AdsImpl::get_last_shown_ad_notification() const {
  return last_shown_ad_notification_;
}

void AdsImpl::set_last_shown_ad_notification(
    const AdNotificationInfo& info) {
  last_shown_ad_notification_ = info;
}

void AdsImpl::StartSustainingAdNotificationInteraction() {
  const uint64_t delay = kSustainAdNotificationInteractionAfterSeconds;

  const base::Time time = sustain_ad_notification_interaction_timer_.Start(
      delay, base::BindOnce(&AdsImpl::SustainAdNotificationInteractionIfNeeded,
          base::Unretained(this)));

  BLOG(1, "Start timer to sustain ad for "
      << last_shown_ad_notification_.target_url << " which will trigger "
          << FriendlyDateAndTime(time));
}

void AdsImpl::SustainAdNotificationInteractionIfNeeded() {
  if (!IsStillViewingAdNotification()) {
    BLOG(1, "Failed to sustain ad. The domain for the focused tab does not "
        "match " << last_shown_ad_notification_.target_url << " for the last "
            "shown ad notification");

    return;
  }

  BLOG(1, "Sustained ad for " << last_shown_ad_notification_.target_url);

  ConfirmAd(last_shown_ad_notification_, ConfirmationType::kLanded);
}

bool AdsImpl::IsStillViewingAdNotification() const {
  return SameSite(active_tab_url_, last_shown_ad_notification_.target_url);
}

void AdsImpl::ConfirmAd(
    const AdInfo& info,
    const ConfirmationType confirmation_type) {
  const Reports reports(this);
  const std::string report = reports.GenerateConfirmationEventReport(
      info.creative_instance_id, confirmation_type);
  BLOG(3, "Event log: " << report);

  ads_client_->ConfirmAd(info, confirmation_type);
}

void AdsImpl::ConfirmAction(
    const std::string& creative_instance_id,
    const std::string& creative_set_id,
    const ConfirmationType confirmation_type) {
  const Reports reports(this);
  const std::string report = reports.GenerateConfirmationEventReport(
      creative_instance_id, confirmation_type);
  BLOG(3, "Event log: " << report);

  ads_client_->ConfirmAction(creative_instance_id, creative_set_id,
      confirmation_type);
}

void AdsImpl::AppendAdNotificationToHistory(
    const AdNotificationInfo& info,
    const ConfirmationType& confirmation_type) {
  AdHistory ad_history;
  ad_history.timestamp_in_seconds =
      static_cast<uint64_t>(base::Time::Now().ToDoubleT());
  ad_history.uuid = base::GenerateGUID();
  ad_history.parent_uuid = info.parent_uuid;
  ad_history.ad_content.creative_instance_id = info.creative_instance_id;
  ad_history.ad_content.creative_set_id = info.creative_set_id;
  ad_history.ad_content.brand = info.title;
  ad_history.ad_content.brand_info = info.body;
  ad_history.ad_content.brand_display_url = GetDisplayUrl(info.target_url);
  ad_history.ad_content.brand_url = info.target_url;
  ad_history.ad_content.ad_action = confirmation_type;
  ad_history.category_content.category = info.category;

  client_->AppendAdHistoryToAdsHistory(ad_history);
}

//////////////////////////////////////////////////////////////////////////////

std::string AdsImpl::ToJSON() const {
  DCHECK(state_has_loaded_);

  base::Value dictionary(base::Value::Type::DICTIONARY);

  // Catalog issuers
  auto catalog_issuers =
      GetCatalogIssuersAsDictionary(public_key_, catalog_issuers_);
  dictionary.SetKey("catalog_issuers", base::Value(std::move(catalog_issuers)));

  // Next token redemption date
  auto token_redemption_timestamp_in_seconds =
      GetNextTokenRedemptionDateInSeconds();
  dictionary.SetKey("next_token_redemption_date_in_seconds", base::Value(
      std::to_string(token_redemption_timestamp_in_seconds)));

  // Confirmations
  auto confirmations = GetConfirmationsAsDictionary(confirmations_);
  dictionary.SetKey("confirmations", base::Value(std::move(confirmations)));

  // Ads rewards
  auto ads_rewards = ad_rewards_->GetAsDictionary();
  dictionary.SetKey("ads_rewards", base::Value(std::move(ads_rewards)));

  // Transaction history
  auto transaction_history =
      GetTransactionHistoryAsDictionary(transaction_history_);
  dictionary.SetKey("transaction_history", base::Value(
      std::move(transaction_history)));

  // Unblinded tokens
  auto unblinded_tokens = unblinded_tokens_->GetTokensAsList();
  dictionary.SetKey("unblinded_tokens", base::Value(
      std::move(unblinded_tokens)));

  // Unblinded payment tokens
  auto unblinded_payment_tokens = unblinded_payment_tokens_->GetTokensAsList();
  dictionary.SetKey("unblinded_payment_tokens", base::Value(
      std::move(unblinded_payment_tokens)));

  // Write to JSON
  std::string json;
  base::JSONWriter::Write(dictionary, &json);

  return json;
}

base::Value AdsImpl::GetCatalogIssuersAsDictionary(
    const std::string& public_key,
    const std::map<std::string, std::string>& issuers) const {
  base::Value dictionary(base::Value::Type::DICTIONARY);
  dictionary.SetKey("public_key", base::Value(public_key));

  base::Value list(base::Value::Type::LIST);
  for (const auto& issuer : issuers) {
    base::Value issuer_dictionary(base::Value::Type::DICTIONARY);

    issuer_dictionary.SetKey("name", base::Value(issuer.second));
    issuer_dictionary.SetKey("public_key", base::Value(issuer.first));

    list.Append(std::move(issuer_dictionary));
  }

  dictionary.SetKey("issuers", base::Value(std::move(list)));

  return dictionary;
}

base::Value AdsImpl::GetConfirmationsAsDictionary(
    const ConfirmationList& confirmations) const {
  base::Value dictionary(base::Value::Type::DICTIONARY);

  base::Value list(base::Value::Type::LIST);
  for (const auto& confirmation : confirmations) {
    base::Value confirmation_dictionary(base::Value::Type::DICTIONARY);

    confirmation_dictionary.SetKey("id", base::Value(confirmation.id));

    confirmation_dictionary.SetKey("creative_instance_id",
        base::Value(confirmation.creative_instance_id));

    std::string type = std::string(confirmation.type);
    confirmation_dictionary.SetKey("type", base::Value(type));

    base::Value token_info_dictionary(base::Value::Type::DICTIONARY);
    auto unblinded_token_base64 =
        confirmation.token_info.unblinded_token.encode_base64();
    token_info_dictionary.SetKey("unblinded_token",
        base::Value(unblinded_token_base64));
    auto public_key = confirmation.token_info.public_key;
    token_info_dictionary.SetKey("public_key", base::Value(public_key));
    confirmation_dictionary.SetKey("token_info",
        base::Value(std::move(token_info_dictionary)));

    auto payment_token_base64 = confirmation.payment_token.encode_base64();
    confirmation_dictionary.SetKey("payment_token",
        base::Value(payment_token_base64));

    auto blinded_payment_token_base64 =
        confirmation.blinded_payment_token.encode_base64();
    confirmation_dictionary.SetKey("blinded_payment_token",
        base::Value(blinded_payment_token_base64));

    confirmation_dictionary.SetKey("credential",
        base::Value(confirmation.credential));

    confirmation_dictionary.SetKey("timestamp_in_seconds",
        base::Value(std::to_string(confirmation.timestamp_in_seconds)));

    confirmation_dictionary.SetKey("created",
        base::Value(confirmation.created));

    list.Append(std::move(confirmation_dictionary));
  }

  dictionary.SetKey("failed_confirmations", base::Value(std::move(list)));

  return dictionary;
}

base::Value AdsImpl::GetTransactionHistoryAsDictionary(
    const TransactionList& transaction_history) const {
  base::Value dictionary(base::Value::Type::DICTIONARY);

  base::Value list(base::Value::Type::LIST);
  for (const auto& transaction : transaction_history) {
    base::Value transaction_dictionary(base::Value::Type::DICTIONARY);

    transaction_dictionary.SetKey("timestamp_in_seconds",
        base::Value(std::to_string(transaction.timestamp_in_seconds)));

    transaction_dictionary.SetKey("estimated_redemption_value",
        base::Value(transaction.estimated_redemption_value));

    transaction_dictionary.SetKey("confirmation_type",
        base::Value(transaction.confirmation_type));

    list.Append(std::move(transaction_dictionary));
  }

  dictionary.SetKey("transactions", base::Value(std::move(list)));

  return dictionary;
}

bool AdsImpl::FromJSON(const std::string& json) {
  DCHECK(state_has_loaded_);

  base::Optional<base::Value> value = base::JSONReader::Read(json);
  if (!value || !value->is_dict()) {
    return false;
  }

  base::DictionaryValue* dictionary = nullptr;
  if (!value->GetAsDictionary(&dictionary)) {
    return false;
  }

  if (!ParseCatalogIssuersFromJSON(dictionary)) {
    BLOG(0, "Failed to parse catalog issuers");
  }

  if (!ParseNextTokenRedemptionDateInSecondsFromJSON(dictionary)) {
    BLOG(0, "Failed to parse next token redemption date in seconds");
  }

  if (!ParseConfirmationsFromJSON(dictionary)) {
    BLOG(0, "Failed to parse confirmations");
  }

  if (!ad_rewards_->SetFromDictionary(dictionary)) {
    BLOG(0, "Failed to parse ads rewards");
  }

  if (!ParseTransactionHistoryFromJSON(dictionary)) {
    BLOG(0, "Failed to parse transaction history");
  }

  if (!ParseUnblindedTokensFromJSON(dictionary)) {
    BLOG(0, "Failed to parse unblinded tokens");
  }

  if (!ParseUnblindedPaymentTokensFromJSON(dictionary)) {
    BLOG(0, "Failed to parse unblinded payment tokens");
  }

  return true;
}

bool AdsImpl::ParseCatalogIssuersFromJSON(
    base::DictionaryValue* dictionary) {
  DCHECK(dictionary);
  if (!dictionary) {
    return false;
  }

  auto* catalog_issuers_value = dictionary->FindKey("catalog_issuers");
  if (!catalog_issuers_value) {
    return false;
  }

  base::DictionaryValue* catalog_issuers_dictionary;
  if (!catalog_issuers_value->GetAsDictionary(&catalog_issuers_dictionary)) {
    return false;
  }

  std::string public_key;
  std::map<std::string, std::string> catalog_issuers;
  if (!GetCatalogIssuersFromDictionary(catalog_issuers_dictionary, &public_key,
      &catalog_issuers)) {
    return false;
  }

  public_key_ = public_key;
  catalog_issuers_ = catalog_issuers;

  return true;
}

bool AdsImpl::GetCatalogIssuersFromDictionary(
    base::DictionaryValue* dictionary,
    std::string* public_key,
    std::map<std::string, std::string>* issuers) const {
  DCHECK(dictionary);
  if (!dictionary) {
    return false;
  }

  DCHECK(public_key);
  if (!public_key) {
    return false;
  }

  DCHECK(issuers);
  if (!issuers) {
    return false;
  }

  // Public key
  auto* public_key_value = dictionary->FindKey("public_key");
  if (!public_key_value) {
    return false;
  }
  *public_key = public_key_value->GetString();

  // Issuers
  auto* issuers_value = dictionary->FindKey("issuers");
  if (!issuers_value) {
    return false;
  }

  issuers->clear();
  base::ListValue issuers_list_value(issuers_value->GetList());
  for (auto& issuer_value : issuers_list_value) {
    base::DictionaryValue* issuer_dictionary;
    if (!issuer_value.GetAsDictionary(&issuer_dictionary)) {
      return false;
    }

    // Public key
    auto* public_key_value = issuer_dictionary->FindKey("public_key");
    if (!public_key_value) {
      return false;
    }
    auto public_key = public_key_value->GetString();

    // Name
    auto* name_value = issuer_dictionary->FindKey("name");
    if (!name_value) {
      return false;
    }
    auto name = name_value->GetString();

    issuers->insert({public_key, name});
  }

  return true;
}

bool AdsImpl::ParseNextTokenRedemptionDateInSecondsFromJSON(
    base::DictionaryValue* dictionary) {
  DCHECK(dictionary);
  if (!dictionary) {
    return false;
  }

  auto* next_token_redemption_date_in_seconds_value =
      dictionary->FindKey("next_token_redemption_date_in_seconds");
  if (!next_token_redemption_date_in_seconds_value) {
    return false;
  }

  uint64_t next_token_redemption_date_in_seconds;
  if (!base::StringToUint64(
      next_token_redemption_date_in_seconds_value->GetString(),
          &next_token_redemption_date_in_seconds)) {
    return false;
  }

  redeem_unblinded_payment_tokens_->set_token_redemption_timestamp(
      MigrateTimestampToDoubleT(next_token_redemption_date_in_seconds));

  return true;
}

bool AdsImpl::ParseConfirmationsFromJSON(
    base::DictionaryValue* dictionary) {
  DCHECK(dictionary);
  if (!dictionary) {
    return false;
  }

  auto* confirmations_value = dictionary->FindKey("confirmations");
  if (!confirmations_value) {
    return false;
  }

  base::DictionaryValue* confirmations_dictionary;
  if (!confirmations_value->GetAsDictionary(
        &confirmations_dictionary)) {
    return false;
  }

  ConfirmationList confirmations;
  if (!GetConfirmationsFromDictionary(confirmations_dictionary,
      &confirmations)) {
    return false;
  }

  confirmations_ = confirmations;

  return true;
}

bool AdsImpl::GetConfirmationsFromDictionary(
    base::DictionaryValue* dictionary,
    ConfirmationList* confirmations) {
  DCHECK(dictionary);
  if (!dictionary) {
    return false;
  }

  DCHECK(confirmations);
  if (!confirmations) {
    return false;
  }

  // Confirmations
  auto* confirmations_value = dictionary->FindKey("failed_confirmations");
  if (!confirmations_value) {
    DCHECK(false) << "Confirmations dictionary missing confirmations";
    return false;
  }

  confirmations->clear();
  base::ListValue confirmations_list_value(confirmations_value->GetList());
  for (auto& confirmation_value : confirmations_list_value) {
    base::DictionaryValue* confirmation_dictionary;
    if (!confirmation_value.GetAsDictionary(&confirmation_dictionary)) {
      DCHECK(false) << "Confirmation should be a dictionary";
      continue;
    }

    ConfirmationInfo confirmation_info;

    // Id
    auto* id_value = confirmation_dictionary->FindKey("id");
    if (id_value) {
      confirmation_info.id = id_value->GetString();
    } else {
      // Id missing, skip confirmation
      DCHECK(false) << "Confirmation missing id";
      continue;
    }

    // Creative instance id
    auto* creative_instance_id_value =
        confirmation_dictionary->FindKey("creative_instance_id");
    if (creative_instance_id_value) {
      confirmation_info.creative_instance_id =
          creative_instance_id_value->GetString();
    } else {
      // Creative instance id missing, skip confirmation
      DCHECK(false) << "Confirmation missing creative_instance_id";
      continue;
    }

    // Type
    auto* type_value = confirmation_dictionary->FindKey("type");
    if (type_value) {
      ConfirmationType type(type_value->GetString());
      confirmation_info.type = type;
    } else {
      // Type missing, skip confirmation
      DCHECK(false) << "Confirmation missing type";
      continue;
    }

    // Token info
    auto* token_info_value = confirmation_dictionary->FindKey("token_info");
    if (!token_info_value) {
      DCHECK(false) << "Confirmation missing token_info";
      continue;
    }

    base::DictionaryValue* token_info_dictionary;
    if (!token_info_value->GetAsDictionary(&token_info_dictionary)) {
      DCHECK(false) << "Token info should be a dictionary";
      continue;
    }

    auto* unblinded_token_value =
        token_info_dictionary->FindKey("unblinded_token");
    if (unblinded_token_value) {
      auto unblinded_token_base64 = unblinded_token_value->GetString();
      confirmation_info.token_info.unblinded_token =
          UnblindedToken::decode_base64(unblinded_token_base64);
    } else {
      // Unblinded token missing, skip confirmation
      DCHECK(false) << "Token info missing unblinded_token";
      continue;
    }

    auto* public_key_value = token_info_dictionary->FindKey("public_key");
    if (public_key_value) {
      confirmation_info.token_info.public_key = public_key_value->GetString();
    } else {
      // Public key missing, skip confirmation
      DCHECK(false) << "Token info missing public_key";
      continue;
    }

    // Payment token
    auto* payment_token_value =
        confirmation_dictionary->FindKey("payment_token");
    if (payment_token_value) {
      auto payment_token_base64 = payment_token_value->GetString();
      confirmation_info.payment_token =
          Token::decode_base64(payment_token_base64);
    } else {
      // Payment token missing, skip confirmation
      DCHECK(false) << "Confirmation missing payment_token";
      continue;
    }

    // Blinded payment token
    auto* blinded_payment_token_value =
        confirmation_dictionary->FindKey("blinded_payment_token");
    if (blinded_payment_token_value) {
      auto blinded_payment_token_base64 =
          blinded_payment_token_value->GetString();
      confirmation_info.blinded_payment_token =
          BlindedToken::decode_base64(blinded_payment_token_base64);
    } else {
      // Blinded payment token missing, skip confirmation
      DCHECK(false) << "Confirmation missing blinded_payment_token";
      continue;
    }

    // Credential
    auto* credential_value = confirmation_dictionary->FindKey("credential");
    if (credential_value) {
      confirmation_info.credential = credential_value->GetString();
    } else {
      // Credential missing, skip confirmation
      DCHECK(false) << "Confirmation missing credential";
      continue;
    }

    // Timestamp
    auto* timestamp_in_seconds_value =
        confirmation_dictionary->FindKey("timestamp_in_seconds");
    if (timestamp_in_seconds_value) {
      uint64_t timestamp_in_seconds;
      if (!base::StringToUint64(timestamp_in_seconds_value->GetString(),
          &timestamp_in_seconds)) {
        continue;
      }

      confirmation_info.timestamp_in_seconds = timestamp_in_seconds;
    }

    // Created
    auto* created_value = confirmation_dictionary->FindKey("created");
    if (created_value) {
      confirmation_info.created = created_value->GetBool();
    } else {
      confirmation_info.created = true;
    }

    confirmations->push_back(confirmation_info);
  }

  return true;
}

bool AdsImpl::ParseTransactionHistoryFromJSON(
    base::DictionaryValue* dictionary) {
  DCHECK(dictionary);
  if (!dictionary) {
    return false;
  }

  auto* transaction_history_value = dictionary->FindKey("transaction_history");
  if (!transaction_history_value) {
    return false;
  }

  base::DictionaryValue* transaction_history_dictionary;
  if (!transaction_history_value->GetAsDictionary(
        &transaction_history_dictionary)) {
    return false;
  }

  TransactionList transaction_history;
  if (!GetTransactionHistoryFromDictionary(transaction_history_dictionary,
      &transaction_history)) {
    return false;
  }

  transaction_history_ = transaction_history;

  return true;
}

bool AdsImpl::GetTransactionHistoryFromDictionary(
    base::DictionaryValue* dictionary,
    TransactionList* transaction_history) {
  DCHECK(dictionary);
  if (!dictionary) {
    return false;
  }

  DCHECK(transaction_history);
  if (!transaction_history) {
    return false;
  }

  // Transaction
  auto* transactions_value = dictionary->FindKey("transactions");
  if (!transactions_value) {
    DCHECK(false) << "Transactions history dictionary missing transactions";
    return false;
  }

  transaction_history->clear();
  base::ListValue transactions_list_value(transactions_value->GetList());
  for (auto& transaction_value : transactions_list_value) {
    base::DictionaryValue* transaction_dictionary;
    if (!transaction_value.GetAsDictionary(&transaction_dictionary)) {
      DCHECK(false) << "Transaction should be a dictionary";
      continue;
    }

    TransactionInfo info;

    // Timestamp
    auto* timestamp_in_seconds_value =
        transaction_dictionary->FindKey("timestamp_in_seconds");
    if (timestamp_in_seconds_value) {
      uint64_t timestamp_in_seconds;
      if (!base::StringToUint64(timestamp_in_seconds_value->GetString(),
          &timestamp_in_seconds)) {
        continue;
      }

      info.timestamp_in_seconds =
          MigrateTimestampToDoubleT(timestamp_in_seconds);
    } else {
      // timestamp missing, fallback to default
      info.timestamp_in_seconds =
          static_cast<uint64_t>(base::Time::Now().ToDoubleT());
    }

    // Estimated redemption value
    auto* estimated_redemption_value_value =
        transaction_dictionary->FindKey("estimated_redemption_value");
    if (estimated_redemption_value_value) {
      info.estimated_redemption_value =
          estimated_redemption_value_value->GetDouble();
    } else {
      // estimated redemption value missing, fallback to default
      info.estimated_redemption_value = 0.0;
    }

    // Confirmation type (>= 0.63.8)
    auto* confirmation_type_value =
        transaction_dictionary->FindKey("confirmation_type");
    if (confirmation_type_value) {
      info.confirmation_type = confirmation_type_value->GetString();
    } else {
      // confirmation type missing, fallback to default
      ConfirmationType type(ConfirmationType::kViewed);
      info.confirmation_type = std::string(type);
    }

    transaction_history->push_back(info);
  }

  return true;
}

bool AdsImpl::ParseUnblindedTokensFromJSON(
    base::DictionaryValue* dictionary) {
  DCHECK(dictionary);
  if (!dictionary) {
    return false;
  }

  auto* unblinded_tokens_value = dictionary->FindKey("unblinded_tokens");
  if (!unblinded_tokens_value) {
    return false;
  }

  base::ListValue unblinded_token_values(unblinded_tokens_value->GetList());

  unblinded_tokens_->SetTokensFromList(unblinded_token_values);

  return true;
}

bool AdsImpl::ParseUnblindedPaymentTokensFromJSON(
    base::DictionaryValue* dictionary) {
  DCHECK(dictionary);
  if (!dictionary) {
    return false;
  }

  auto* unblinded_payment_tokens_value =
      dictionary->FindKey("unblinded_payment_tokens");
  if (!unblinded_payment_tokens_value) {
    return false;
  }

  base::ListValue unblinded_payment_token_values(
      unblinded_payment_tokens_value->GetList());

  unblinded_payment_tokens_->SetTokensFromList(
      unblinded_payment_token_values);

  return true;
}

void AdsImpl::SaveState() {
  if (!state_has_loaded_) {
    NOTREACHED();
    return;
  }

  BLOG(3, "Saving confirmations state");

  std::string json = ToJSON();
  auto callback = std::bind(&AdsImpl::OnStateSaved, this, _1);
  ads_client_->Save(_confirmations_resource_name, json, callback);
}

void AdsImpl::OnStateSaved(const Result result) {
  if (result != SUCCESS) {
    BLOG(0, "Failed to save confirmations state");
    return;
  }

  BLOG(3, "Successfully saved confirmations state");
}

void AdsImpl::LoadState() {
  BLOG(3, "Loading confirmations state");

  auto callback = std::bind(&AdsImpl::OnStateLoaded, this, _1, _2);
  ads_client__->Load(_confirmations_resource_name, callback);
}

void AdsImpl::OnStateLoaded(
    Result result,
    const std::string& json) {
  state_has_loaded_ = true;

  auto confirmations_json = json;

  if (result != SUCCESS) {
    BLOG(3, "Confirmations state does not exist, creating default state");

    confirmations_json = ToJSON();
  } else {
    BLOG(3, "Successfully loaded confirmations state");
  }

  if (!FromJSON(confirmations_json)) {
    state_has_loaded_ = false;

    BLOG(0, "Failed to parse confirmations state: " << confirmations_json);

    ads_client_->ConfirmationsTransactionHistoryDidChange();

    initialize_callback_(false);

    return;
  }

  initialize_callback_(true);
}

void AdsImpl::SetWalletInfo(std::unique_ptr<WalletInfo> info) {
  if (!state_has_loaded_) {
    return;
  }

  if (!info->IsValid()) {
    BLOG(0, "Failed to initialize Confirmations due to invalid wallet");
    return;
  }

  if (*info == wallet_info_) {
    return;
  }

  wallet_info_ = WalletInfo(*info);

  BLOG(1, "SetWalletInfo:\n"
      << "  Payment id: " << wallet_info_.payment_id << "\n"
      << "  Private key: ********");

  UpdateAdsRewards(true);

  MaybeStart();
}

void AdsImpl::SetCatalogIssuers(std::unique_ptr<IssuersInfo> info) {
  DCHECK(state_has_loaded_);
  if (!state_has_loaded_) {
    BLOG(0, "Failed to set catalog issuers as not initialized");
    return;
  }

  BLOG(1, "SetCatalogIssuers:");
  BLOG(1, "  Public key: " << info->public_key);
  BLOG(1, "  Issuers:");

  for (const auto& issuer : info->issuers) {
    BLOG(1, "    Name: " << issuer.name);
    BLOG(1, "    Public key: " << issuer.public_key);
  }

  const bool public_key_was_rotated =
      !public_key_.empty() && public_key_ != info->public_key;

  public_key_ = info->public_key;

  catalog_issuers_.clear();
  for (const auto& issuer : info->issuers) {
    catalog_issuers_.insert({issuer.public_key, issuer.name});
  }

  if (public_key_was_rotated) {
    unblinded_tokens_->RemoveAllTokens();
    if (is_initialized_) {
      RefillUnblindedTokensIfNecessary();
    }
  }

  MaybeStart();
}

std::map<std::string, std::string> AdsImpl::GetCatalogIssuers()
    const {
  DCHECK(state_has_loaded_);

  return catalog_issuers_;
}

bool AdsImpl::IsValidPublicKeyForCatalogIssuers(
    const std::string& public_key) const {
  DCHECK(state_has_loaded_);

  auto it = catalog_issuers_.find(public_key);
  if (it == catalog_issuers_.end()) {
    return false;
  }

  return true;
}

void AdsImpl::AppendConfirmationToQueue(
    const ConfirmationInfo& confirmation_info) {
  DCHECK(state_has_loaded_);

  confirmations_.push_back(confirmation_info);

  SaveState();

  BLOG(1, "Added confirmation id " << confirmation_info.id
      << ", creative instance id " << confirmation_info.creative_instance_id
          << " and " << std::string(confirmation_info.type)
              << " to the confirmations queue");

  StartRetryingFailedConfirmations();
}

void AdsImpl::RemoveConfirmationFromQueue(
    const ConfirmationInfo& confirmation_info) {
  DCHECK(state_has_loaded_);

  auto it = std::find_if(confirmations_.begin(), confirmations_.end(),
      [=](const ConfirmationInfo& info) {
        return (info.id == confirmation_info.id);
      });

  if (it == confirmations_.end()) {
    BLOG(0, "Failed to remove confirmation id " << confirmation_info.id
        << ", creative instance id " << confirmation_info.creative_instance_id
            << " and " << std::string(confirmation_info.type) << " from "
                "the confirmations queue");

    return;
  }

  BLOG(1, "Removed confirmation id " << confirmation_info.id
      << ", creative instance id " << confirmation_info.creative_instance_id
          << " and " << std::string(confirmation_info.type) << " from the "
              "confirmations queue");

  confirmations_.erase(it);

  SaveState();
}

void AdsImpl::UpdateAdsRewards(const bool should_refresh) {
  DCHECK(state_has_loaded_);
  if (!state_has_loaded_) {
    BLOG(0, "Failed to update ads rewards as not initialized");
    return;
  }

  ad_rewards_->Update(wallet_info_, should_refresh);
}

void AdsImpl::UpdateAdsRewards(
    const double estimated_pending_rewards,
    const uint64_t next_payment_date_in_seconds) {
  DCHECK(state_has_loaded_);

  estimated_pending_rewards_ = estimated_pending_rewards;
  next_payment_date_in_seconds_ = next_payment_date_in_seconds;

  SaveState();

  ads_client_->ConfirmationsTransactionHistoryDidChange();
}

void AdsImpl::GetTransactionHistory(
    OnGetTransactionHistoryCallback callback) {
  DCHECK(state_has_loaded_);
  if (!state_has_loaded_) {
    BLOG(0, "Failed to get transaction history as not initialized");
    return;
  }

  auto unredeemed_transactions = GetUnredeemedTransactions();
  double unredeemed_estimated_pending_rewards =
      GetEstimatedPendingRewardsForTransactions(unredeemed_transactions);

  auto all_transactions = GetTransactions();
  uint64_t ad_notifications_received_this_month =
      GetAdNotificationsReceivedThisMonthForTransactions(all_transactions);

  auto transactions_info = std::make_unique<TransactionsInfo>();

  transactions_info->estimated_pending_rewards =
      estimated_pending_rewards_ + unredeemed_estimated_pending_rewards;

  transactions_info->next_payment_date_in_seconds =
      next_payment_date_in_seconds_;

  transactions_info->ad_notifications_received_this_month =
      ad_notifications_received_this_month;

  auto to_timestamp_in_seconds =
      static_cast<uint64_t>(base::Time::Now().ToDoubleT());
  auto transactions = GetTransactionHistory(0, to_timestamp_in_seconds);
  transactions_info->transactions = transactions;

  callback(std::move(transactions_info));
}

void AdsImpl::AddUnredeemedTransactionsToPendingRewards() {
  auto unredeemed_transactions = GetUnredeemedTransactions();
  AddTransactionsToPendingRewards(unredeemed_transactions);
}

void AdsImpl::AddTransactionsToPendingRewards(
    const TransactionList& transactions) {
  estimated_pending_rewards_ +=
      GetEstimatedPendingRewardsForTransactions(transactions);

  ads_client_->ConfirmationsTransactionHistoryDidChange();
}

double AdsImpl::GetEstimatedPendingRewardsForTransactions(
    const TransactionList& transactions) const {
  double estimated_pending_rewards = 0.0;

  for (const auto& transaction : transactions) {
    auto estimated_redemption_value = transaction.estimated_redemption_value;
    if (estimated_redemption_value > 0.0) {
      estimated_pending_rewards += estimated_redemption_value;
    }
  }

  return estimated_pending_rewards;
}

uint64_t AdsImpl::GetAdNotificationsReceivedThisMonthForTransactions(
    const TransactionList& transactions) const {
  uint64_t ad_notifications_received_this_month = 0;

  auto now = base::Time::Now();
  base::Time::Exploded now_exploded;
  now.UTCExplode(&now_exploded);

  for (const auto& transaction : transactions) {
    if (transaction.timestamp_in_seconds == 0) {
      // Workaround for Windows crash when passing 0 to UTCExplode
      continue;
    }

    auto transaction_timestamp =
        base::Time::FromDoubleT(transaction.timestamp_in_seconds);

    base::Time::Exploded transaction_timestamp_exploded;
    transaction_timestamp.UTCExplode(&transaction_timestamp_exploded);

    if (transaction_timestamp_exploded.year == now_exploded.year &&
        transaction_timestamp_exploded.month == now_exploded.month &&
        transaction.estimated_redemption_value > 0.0 &&
        ConfirmationType(transaction.confirmation_type) ==
            ConfirmationType::kViewed) {
      ad_notifications_received_this_month++;
    }
  }

  return ad_notifications_received_this_month;
}

TransactionList AdsImpl::GetTransactionHistory(
    const uint64_t from_timestamp_in_seconds,
    const uint64_t to_timestamp_in_seconds) {
  DCHECK(state_has_loaded_);

  TransactionList transactions(transaction_history_.size());

  auto it = std::copy_if(transaction_history_.begin(),
      transaction_history_.end(), transactions.begin(),
      [=](TransactionInfo& info) {
        return info.timestamp_in_seconds >= from_timestamp_in_seconds &&
            info.timestamp_in_seconds <= to_timestamp_in_seconds;
      });

  transactions.resize(std::distance(transactions.begin(), it));

  return transactions;
}

TransactionList AdsImpl::GetTransactions() const {
  DCHECK(state_has_loaded_);

  return transaction_history_;
}

TransactionList AdsImpl::GetUnredeemedTransactions() {
  DCHECK(state_has_loaded_);

  auto count = unblinded_payment_tokens_->Count();
  if (count == 0) {
    // There are no outstanding unblinded payment tokens to redeem
    return {};
  }

  // Unredeemed transactions are always at the end of the transaction history
  TransactionList transactions(transaction_history_.end() - count,
      transaction_history_.end());

  return transactions;
}

double AdsImpl::GetEstimatedRedemptionValue(
    const std::string& public_key) const {
  DCHECK(state_has_loaded_);

  double estimated_redemption_value = 0.0;

  auto it = catalog_issuers_.find(public_key);
  if (it != catalog_issuers_.end()) {
    auto name = it->second;
    if (!re2::RE2::Replace(&name, "BAT", "")) {
      BLOG(1, "Failed to estimate redemption value due to invalid catalog "
          "issuer name");
    }

    estimated_redemption_value = stod(name);
  }

  return estimated_redemption_value;
}

void AdsImpl::AppendTransactionToHistory(
    const double estimated_redemption_value,
    const ConfirmationType confirmation_type) {
  DCHECK(state_has_loaded_);

  TransactionInfo info;
  info.timestamp_in_seconds =
      static_cast<uint64_t>(base::Time::Now().ToDoubleT());
  info.estimated_redemption_value = estimated_redemption_value;
  info.confirmation_type = std::string(confirmation_type);

  transaction_history_.push_back(info);

  SaveState();

  ads_client_->ConfirmationsTransactionHistoryDidChange();
}

void AdsImpl::ConfirmAd(
    const AdInfo& info,
    const ConfirmationType confirmation_type) {
  if (!state_has_loaded_) {
    BLOG(0, "Failed to confirm ad as not initialized");
    return;
  }

  BLOG(1, "Confirm ad:\n"
      << "  creativeInstanceId: " << info.creative_instance_id << "\n"
      << "  creativeSetId: " << info.creative_set_id << "\n"
      << "  category: " << info.category << "\n"
      << "  targetUrl: " << info.target_url << "\n"
      << "  geoTarget: " << info.geo_target << "\n"
      << "  confirmationType: " << std::string(confirmation_type));

  redeem_unblinded_token_->Redeem(info, confirmation_type);
}

void AdsImpl::ConfirmAction(
    const std::string& creative_instance_id,
    const std::string& creative_set_id,
    const ConfirmationType confirmation_type) {
  DCHECK(state_has_loaded_);
  if (!state_has_loaded_) {
    BLOG(0, "Failed to confirm action as not initialized");
    return;
  }

  BLOG(1, "Confirm action:\n"
      << "  creativeInstanceId: " << creative_instance_id << "\n"
      << "  creativeSetId: " << creative_set_id << "\n"
      << "  confirmationType: " << std::string(confirmation_type));

  redeem_unblinded_token_->Redeem(creative_instance_id, creative_set_id,
      confirmation_type);
}

void AdsImpl::RefillUnblindedTokensIfNecessary() const {
  DCHECK(wallet_info_.IsValid());

  refill_unblinded_tokens_->Refill(wallet_info_, public_key_);
}

uint64_t AdsImpl::GetNextTokenRedemptionDateInSeconds() const {
  return redeem_unblinded_payment_tokens_->get_token_redemption_timestamp();
}

void AdsImpl::StartRetryingFailedConfirmations() {
  if (failed_confirmations_timer_.IsRunning()) {
    return;
  }

  const base::Time time = failed_confirmations_timer_.StartWithPrivacy(
      kRetryFailedConfirmationsAfterSeconds,
          base::BindOnce(&AdsImpl::RetryFailedConfirmations,
              base::Unretained(this)));

  BLOG(1, "Retry failed confirmations " << FriendlyDateAndTime(time));
}

void AdsImpl::RetryFailedConfirmations() {
  if (confirmations_.empty()) {
    BLOG(1, "No failed confirmations to retry");
    return;
  }

  ConfirmationInfo confirmation_info(confirmations_.front());
  RemoveConfirmationFromQueue(confirmation_info);

  redeem_unblinded_token_->Redeem(confirmation_info);

  StartRetryingFailedConfirmations();
}

void AdsImpl::OnDidRedeemUnblindedToken(
    const ConfirmationInfo& confirmation) {
  BLOG(1, "Successfully redeemed unblinded token with confirmation id "
      << confirmation.id << ", creative instance id "
          << confirmation.creative_instance_id << " and "
              << std::string(confirmation.type));
}

void AdsImpl::OnFailedToRedeemUnblindedToken(
    const ConfirmationInfo& confirmation) {
  BLOG(1, "Failed to redeem unblinded token with confirmation id "
      << confirmation.id << ", creative instance id "
          <<  confirmation.creative_instance_id << " and "
              << std::string(confirmation.type));
}

void AdsImpl::OnDidRedeemUnblindedPaymentTokens() {
  BLOG(1, "Successfully redeemed unblinded payment tokens");
}

void AdsImpl::OnFailedToRedeemUnblindedPaymentTokens() {
  BLOG(1, "Failed to redeem unblinded payment tokens");
}

void AdsImpl::OnDidRetryRedeemingUnblindedPaymentTokens() {
  BLOG(1, "Retry redeeming unblinded payment tokens");
}

void AdsImpl::OnDidRefillUnblindedTokens() {
  BLOG(1, "Successfully refilled unblinded tokens");
}

void AdsImpl::OnFailedToRefillUnblindedTokens() {
  BLOG(1, "Failed to refill unblinded tokens");
}

void AdsImpl::OnDidRetryRefillingUnblindedTokens() {
  BLOG(1, "Retry refilling unblinded tokens");
}

}  // namespace ads
