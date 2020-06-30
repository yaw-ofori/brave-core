/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BAT_ADS_INTERNAL_ADS_IMPL_H_
#define BAT_ADS_INTERNAL_ADS_IMPL_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/values.h"
#include "bat/ads/ad_notification_info.h"
#include "bat/ads/ads_history.h"
#include "bat/ads/ads.h"
#include "bat/ads/confirmation_type.h"
#include "bat/ads/internal/ad_notifications/ad_notifications.h"
#include "bat/ads/internal/bundle/creative_ad_notification_info.h"
#include "bat/ads/internal/classification/page_classifier/page_classifier.h"
#include "bat/ads/internal/classification/purchase_intent_classifier/purchase_intent_classifier.h"
#include "bat/ads/internal/classification/purchase_intent_classifier/purchase_intent_signal_info.h"
#include "bat/ads/internal/client_state/client.h"
#include "bat/ads/internal/confirmation/confirmation_info.h"
#include "bat/ads/internal/database/database_initialize.h"
#include "bat/ads/internal/database/tables/creative_ad_notifications_database_table.h"
#include "bat/ads/internal/privacy/unblinded_tokens/unblinded_tokens.h"
#include "bat/ads/internal/server/ad_rewards/ad_rewards.h"
#include "bat/ads/internal/server/get_catalog/get_catalog.h"
#include "bat/ads/internal/server/redeem_unblinded_payment_tokens/redeem_unblinded_payment_tokens_delegate.h"
#include "bat/ads/internal/server/redeem_unblinded_payment_tokens/redeem_unblinded_payment_tokens.h"
#include "bat/ads/internal/server/redeem_unblinded_token/redeem_unblinded_token_delegate.h"
#include "bat/ads/internal/server/redeem_unblinded_token/redeem_unblinded_token.h"
#include "bat/ads/internal/server/refill_unblinded_tokens/refill_unblinded_tokens_delegate.h"
#include "bat/ads/internal/server/refill_unblinded_tokens/refill_unblinded_tokens.h"
#include "bat/ads/internal/timer.h"
#include "bat/ads/issuers_info.h"
#include "bat/ads/mojom.h"

namespace ads {

class AdConversions;
class AdNotifications;
class Bundle;
class Client;
class ExclusionRule;
class GetCatalog;
class PermissionRule;
class SubdivisionTargeting;

class AdsImpl
    : public Ads,
      public RedeemUnblindedPaymentTokensDelegate,
      public RedeemUnblindedTokenDelegate,
      public RefillUnblindedTokensDelegate {
 public:
  explicit AdsImpl(
      AdsClient* ads_client);
  ~AdsImpl() override;

  AdsClient* get_ads_client() const;
  Client* get_client() const;
  AdNotifications* get_ad_notifications() const;
  SubdivisionTargeting* get_subdivision_targeting() const;
  AdConversions* get_ad_conversions() const;
  classification::PageClassifier* get_page_classifier() const;
  privacy::UnblindedTokens* get_unblinded_tokens() const;
  privacy::UnblindedTokens* get_unblinded_payment_tokens() const;
  Bundle* get_bundle() const;

  InitializeCallback initialize_callback_;
  void Initialize(
      InitializeCallback callback) override;
  void InitializeStep2(
      const Result result);
  void InitializeStep3(
      const Result result);
  void InitializeStep4(
      const Result result);
  void InitializeStep5(
      const Result result);
  void InitializeStep6(
      const Result result);
  bool IsInitialized();

  void Shutdown(
      ShutdownCallback callback) override;

  void LoadUserModel();
  void OnUserModelLoaded(
      const Result result,
      const std::string& json);

  bool IsMobile() const;
  bool IsAndroid() const;

  bool is_foreground_;
  void OnForeground() override;
  void OnBackground() override;
  bool IsForeground() const;

  void OnIdle() override;
  void OnUnIdle() override;

  std::set<int32_t> media_playing_;
  void OnMediaPlaying(
      const int32_t tab_id) override;
  void OnMediaStopped(
      const int32_t tab_id) override;
  bool IsMediaPlaying() const;

  bool GetAdNotification(
      const std::string& uuid,
      AdNotificationInfo* info) override;
  void OnAdNotificationEvent(
      const std::string& uuid,
      const AdNotificationEventType event_type) override;

  bool ShouldNotDisturb() const;

  int32_t active_tab_id_;
  std::string active_tab_url_;
  std::string previous_tab_url_;
  void OnTabUpdated(
      const int32_t tab_id,
      const std::string& url,
      const bool is_active,
      const bool is_incognito) override;
  void OnTabClosed(
      const int32_t tab_id) override;

  void RemoveAllHistory(
      RemoveAllHistoryCallback callback) override;

  AdsHistory GetAdsHistory(
      const AdsHistory::FilterType filter_type,
      const AdsHistory::SortType sort_type,
      const uint64_t from_timestamp,
      const uint64_t to_timestamp) override;

  AdContent::LikeAction ToggleAdThumbUp(
      const std::string& creative_instance_id,
      const std::string& creative_set_id,
      const AdContent::LikeAction& action) override;
  AdContent::LikeAction ToggleAdThumbDown(
      const std::string& creative_instance_id,
      const std::string& creative_set_id,
      const AdContent::LikeAction& action) override;
  CategoryContent::OptAction ToggleAdOptInAction(
      const std::string& category,
      const CategoryContent::OptAction& action) override;
  CategoryContent::OptAction ToggleAdOptOutAction(
      const std::string& category,
      const CategoryContent::OptAction& action) override;
  bool ToggleSaveAd(
      const std::string& creative_instance_id,
      const std::string& creative_set_id,
      const bool saved) override;
  bool ToggleFlagAd(
      const std::string& creative_instance_id,
      const std::string& creative_set_id,
      const bool flagged) override;

  void ChangeLocale(
      const std::string& locale) override;

  void OnAdsSubdivisionTargetingCodeHasChanged() override;

  void OnPageLoaded(
      const std::string& url,
      const std::string& content) override;

  void ExtractPurchaseIntentSignal(
      const std::string& url);
  void GeneratePurchaseIntentSignalHistoryEntry(
      const classification::PurchaseIntentSignalInfo& purchase_intent_signal);
  classification::PurchaseIntentWinningCategoryList
      GetWinningPurchaseIntentCategories();

  void MaybeClassifyPage(
      const std::string& url,
      const std::string& content);

  void MaybeServeAdNotification(
      const bool should_serve);
  void ServeAdNotificationIfReady();
  void ServeAdNotificationFromCategories(
      const classification::CategoryList& categories);
  void OnServeAdNotificationFromCategories(
      const Result result,
      const classification::CategoryList& categories,
      const CreativeAdNotificationList& ads);
  bool ServeAdNotificationFromParentCategories(
      const classification::CategoryList& categories);
  void ServeUntargetedAdNotification();
  void OnServeUntargetedAdNotification(
      const Result result,
      const classification::CategoryList& categories,
      const CreativeAdNotificationList& ads);
  classification::CategoryList GetCategoriesToServeAd();
  void ServeAdNotificationWithPacing(
      const CreativeAdNotificationList& ads);
  void SuccessfullyServedAd();
  void FailedToServeAdNotification(
      const std::string& reason);

  CreativeAdNotificationList GetEligibleAds(
      const CreativeAdNotificationList& ads);
  CreativeAdNotificationList GetUnseenAdsAndRoundRobinIfNeeded(
      const CreativeAdNotificationList& ads) const;
  CreativeAdNotificationList GetUnseenAds(
      const CreativeAdNotificationList& ads) const;
  CreativeAdNotificationList GetAdsForUnseenAdvertisers(
      const CreativeAdNotificationList& ads) const;

  bool IsAdNotificationValid(
      const CreativeAdNotificationInfo& info);
  bool ShowAdNotification(
      const CreativeAdNotificationInfo& info);
  bool IsAllowedToServeAdNotifications();

  Timer deliver_ad_notification_timer_;
  void StartDeliveringAdNotifications();
  void StartDeliveringAdNotificationsAfterSeconds(
      const uint64_t seconds);
  void DeliverAdNotification();
  bool IsCatalogOlderThanOneDay();

  #if defined(OS_ANDROID)
  void RemoveAllAdNotificationsAfterReboot();
  void RemoveAllAdNotificationsAfterUpdate();
  #endif

  const AdNotificationInfo& get_last_shown_ad_notification() const;
  void set_last_shown_ad_notification(
      const AdNotificationInfo& info);

  void ConfirmAd(
      const AdInfo& info,
      const ConfirmationType confirmation_type);

  void ConfirmAction(
      const std::string& creative_instance_id,
      const std::string& creative_set_id,
      const ConfirmationType confirmation_type);

  uint64_t next_easter_egg_timestamp_in_seconds_;

  void AppendAdNotificationToHistory(
      const AdNotificationInfo& info,
      const ConfirmationType& confirmation_type);

  // Wallet
  void SetWalletInfo(std::unique_ptr<WalletInfo> info) override;

  // Catalog issuers
  void SetCatalogIssuers(std::unique_ptr<IssuersInfo> info) override;
  std::map<std::string, std::string> GetCatalogIssuers() const;
  bool IsValidPublicKeyForCatalogIssuers(const std::string& public_key) const;
  double GetEstimatedRedemptionValue(const std::string& public_key) const;

  // Confirmations
  void AppendConfirmationToQueue(const ConfirmationInfo& confirmation_info);
  void StartRetryingFailedConfirmations();

  // Ads rewards
  void UpdateAdsRewards(const bool should_refresh) override;

  void UpdateAdsRewards(
      const double estimated_pending_rewards,
      const uint64_t next_payment_date_in_seconds);

  // Transaction history
  void GetTransactionHistory(
      GetTransactionHistoryCallback callback) override;
  void AddUnredeemedTransactionsToPendingRewards();
  void AddTransactionsToPendingRewards(
      const TransactionList& transactions);
  double GetEstimatedPendingRewardsForTransactions(
      const TransactionList& transactions) const;
  uint64_t GetAdNotificationsReceivedThisMonthForTransactions(
      const TransactionList& transactions) const;
  TransactionList GetTransactionHistory(
      const uint64_t from_timestamp_in_seconds,
      const uint64_t to_timestamp_in_seconds);
  TransactionList GetTransactions() const;
  TransactionList GetUnredeemedTransactions();
  void AppendTransactionToHistory(
      const double estimated_redemption_value,
      const ConfirmationType confirmation_type);

  // Refill tokens
  void StartRetryingToGetRefillSignedTokens(const uint64_t start_timer_in);
  void RefillUnblindedTokensIfNecessary() const;

  // Payout tokens
  uint64_t GetNextTokenRedemptionDateInSeconds() const;

  // State
  void SaveState();

  std::unique_ptr<Client> client_;
  std::unique_ptr<Bundle> bundle_;
  std::unique_ptr<GetCatalog> get_catalog_;
  std::unique_ptr<SubdivisionTargeting> subdivision_targeting_;
  std::unique_ptr<AdConversions> ad_conversions_;
  std::unique_ptr<database::Initialize> database_;
  std::unique_ptr<classification::PageClassifier> page_classifier_;
  std::unique_ptr<classification::PurchaseIntentClassifier>
      purchase_intent_classifier_;

 private:
  bool is_initialized_;

  bool is_confirmations_ready_;

  AdNotificationInfo last_shown_ad_notification_;
  CreativeAdNotificationInfo last_shown_creative_ad_notification_;
  Timer sustain_ad_notification_interaction_timer_;
  AdNotificationInfo last_sustained_ad_notification_;
  void StartSustainingAdNotificationInteraction();
  void SustainAdNotificationInteractionIfNeeded();
  bool IsStillViewingAdNotification() const;

  std::unique_ptr<AdNotifications> ad_notifications_;

  AdsClient* ads_client_;  // NOT OWNED

  std::vector<std::unique_ptr<PermissionRule>> CreatePermissionRules() const;

  std::vector<std::unique_ptr<ExclusionRule>> CreateExclusionRules() const;

  // Wallet
  WalletInfo wallet_info_;
  std::string public_key_;

  // Catalog issuers
  std::map<std::string, std::string> catalog_issuers_;

  // Confirmations
  Timer failed_confirmations_timer_;
  void RemoveConfirmationFromQueue(const ConfirmationInfo& confirmation_info);
  void RetryFailedConfirmations();
  ConfirmationList confirmations_;

  // Transaction history
  TransactionList transaction_history_;

  // Unblinded tokens
  std::unique_ptr<privacy::UnblindedTokens> unblinded_tokens_;
  std::unique_ptr<privacy::UnblindedTokens> unblinded_payment_tokens_;

  // Ads rewards
  double estimated_pending_rewards_;
  uint64_t next_payment_date_in_seconds_;
  std::unique_ptr<AdRewards> ad_rewards_;

  std::unique_ptr<RefillUnblindedTokens> refill_unblinded_tokens_;

  std::unique_ptr<RedeemUnblindedToken> redeem_unblinded_token_;

  std::unique_ptr<RedeemUnblindedPaymentTokens>
      redeem_unblinded_payment_tokens_;

  // State
  void OnStateSaved(const Result result);

  bool state_has_loaded_;
  void LoadState();
  void OnStateLoaded(Result result, const std::string& json);

  std::string ToJSON() const;

  base::Value GetCatalogIssuersAsDictionary(
      const std::string& public_key,
      const std::map<std::string, std::string>& issuers) const;

  base::Value GetConfirmationsAsDictionary(
      const ConfirmationList& confirmations) const;

  base::Value GetTransactionHistoryAsDictionary(
      const TransactionList& transaction_history) const;

  bool FromJSON(const std::string& json);

  bool ParseCatalogIssuersFromJSON(
      base::DictionaryValue* dictionary);
  bool GetCatalogIssuersFromDictionary(
      base::DictionaryValue* dictionary,
      std::string* public_key,
      std::map<std::string, std::string>* issuers) const;

  bool ParseNextTokenRedemptionDateInSecondsFromJSON(
      base::DictionaryValue* dictionary);

  bool ParseConfirmationsFromJSON(
      base::DictionaryValue* dictionary);
  bool GetConfirmationsFromDictionary(
      base::DictionaryValue* dictionary,
      ConfirmationList* confirmations);

  bool ParseTransactionHistoryFromJSON(
      base::DictionaryValue* dictionary);
  bool GetTransactionHistoryFromDictionary(
      base::DictionaryValue* dictionary,
      TransactionList* transaction_history);

  bool ParseUnblindedTokensFromJSON(
      base::DictionaryValue* dictionary);

  bool ParseUnblindedPaymentTokensFromJSON(
      base::DictionaryValue* dictionary);

  // RedeemTokenDelegate implementation
  void OnDidRedeemUnblindedToken(
      const ConfirmationInfo& confirmation) override;
  void OnFailedToRedeemUnblindedToken(
      const ConfirmationInfo& confirmation) override;

  // RedeemUnblindedPaymentTokensDelegate implementation
  void OnDidRedeemUnblindedPaymentTokens() override;
  void OnFailedToRedeemUnblindedPaymentTokens() override;
  void OnDidRetryRedeemingUnblindedPaymentTokens() override;

  // RefillUnblindedTokensDelegate implementation
  void OnDidRefillUnblindedTokens() override;
  void OnFailedToRefillUnblindedTokens() override;
  void OnDidRetryRefillingUnblindedTokens() override;

  // Not copyable, not assignable
  AdsImpl(const AdsImpl&) = delete;
  AdsImpl& operator=(const AdsImpl&) = delete;
};

}  // namespace ads

#endif  // BAT_ADS_INTERNAL_ADS_IMPL_H_
