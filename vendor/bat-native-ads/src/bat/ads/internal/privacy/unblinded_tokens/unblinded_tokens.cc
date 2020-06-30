/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed w
 * h this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ads/internal/privacy/unblinded_tokens/unblinded_tokens.h"

#include <string>
#include <utility>

#include "bat/ads/internal/logging.h"

namespace ads {
namespace privacy {

UnblindedTokens::UnblindedTokens(
    AdsImpl* ads)
    : ads_(ads) {
  DCHECK(ads_);
}

UnblindedTokens::~UnblindedTokens() = default;

UnblindedTokenInfo UnblindedTokens::GetToken() const {
  DCHECK_NE(Count(), 0);

  return unblinded_tokens_.front();
}

UnblindedTokenList UnblindedTokens::GetAllTokens() const {
  return unblinded_tokens_;
}

base::Value UnblindedTokens::GetTokensAsList() {
  base::Value list(base::Value::Type::LIST);

  for (const auto& unblinded_token : unblinded_tokens_) {
    base::Value dictionary(base::Value::Type::DICTIONARY);
    dictionary.SetKey("unblinded_token", base::Value(
        unblinded_token.value.encode_base64()));
    dictionary.SetKey("public_key", base::Value(unblinded_token.public_key));

    list.Append(std::move(dictionary));
  }

  return list;
}

void UnblindedTokens::SetTokens(
    const UnblindedTokenList& unblinded_tokens) {
  unblinded_tokens_ = unblinded_tokens;
  ads_->SaveState();
}

void UnblindedTokens::SetTokensFromList(
    const base::Value& list) {
  base::ListValue list_values(list.GetList());

  UnblindedTokenList unblinded_tokens;
  for (auto& value : list_values) {
    std::string base64_unblinded_token;
    std::string public_key;

    if (value.is_string()) {
      // Migrate legacy tokens
      base64_unblinded_token = value.GetString();
      public_key = "";
    } else {
      base::DictionaryValue* dictionary;
      if (!value.GetAsDictionary(&dictionary)) {
        DCHECK(false) << "Unblinded token should be a dictionary";
        continue;
      }

      // Unblinded token
      auto* unblinded_token_value = dictionary->FindKey("unblinded_token");
      if (!unblinded_token_value) {
        DCHECK(false) << "Unblinded token dictionary missing unblinded_token";
        continue;
      }
      base64_unblinded_token = unblinded_token_value->GetString();

      // Public key
      auto* public_key_value = dictionary->FindKey("public_key");
      if (!public_key_value) {
        DCHECK(false) << "Unblinded token dictionary missing public_key";
        continue;
      }
      public_key = public_key_value->GetString();
    }

    UnblindedTokenInfo unblinded_token;
    unblinded_token.value =
        UnblindedToken::decode_base64(base64_unblinded_token);
    unblinded_token.public_key = public_key;

    unblinded_tokens.push_back(unblinded_token);
  }

  SetTokens(unblinded_tokens);
}

void UnblindedTokens::AddTokens(
    const UnblindedTokenList& unblinded_tokens) {
  for (const auto& unblinded_token : unblinded_tokens) {
    if (TokenExists(unblinded_token)) {
      continue;
    }

    unblinded_tokens_.push_back(unblinded_token);
  }

  ads_->SaveState();
}

bool UnblindedTokens::RemoveToken(
    const UnblindedTokenInfo& unblinded_token) {
  auto iter = std::find_if(unblinded_tokens_.begin(), unblinded_tokens_.end(),
      [&unblinded_token](const UnblindedTokenInfo& value) {
    return (unblinded_token == value);
  });

  if (iter == unblinded_tokens_.end()) {
    return false;
  }

  unblinded_tokens_.erase(iter);

  ads_->SaveState();

  return true;
}

void UnblindedTokens::RemoveAllTokens() {
  unblinded_tokens_.clear();
  ads_->SaveState();
}

bool UnblindedTokens::TokenExists(
    const UnblindedTokenInfo& unblinded_token) {
  auto iter = std::find_if(unblinded_tokens_.begin(), unblinded_tokens_.end(),
      [&unblinded_token](const UnblindedTokenInfo& value) {
    return (unblinded_token == value);
  });

  if (iter == unblinded_tokens_.end()) {
    return false;
  }

  return true;
}

int UnblindedTokens::Count() const {
  return unblinded_tokens_.size();
}

bool UnblindedTokens::IsEmpty() const {
  return unblinded_tokens_.empty();
}

}  // namespace privacy
}  // namespace ads
