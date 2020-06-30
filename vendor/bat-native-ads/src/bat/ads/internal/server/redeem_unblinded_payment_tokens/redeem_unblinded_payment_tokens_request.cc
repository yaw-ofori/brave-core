/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ads/internal/server/redeem_unblinded_payment_tokens/redeem_unblinded_payment_tokens_request.h"

#include <utility>

#include "base/json/json_writer.h"
#include "bat/ads/internal/privacy/unblinded_tokens/unblinded_token_info.h"
#include "bat/ads/internal/server/ads_server_util.h"
#include "bat/ads/internal/logging.h"

namespace ads {

RedeemUnblindedPaymentTokensRequest::
RedeemUnblindedPaymentTokensRequest() = default;

RedeemUnblindedPaymentTokensRequest::
~RedeemUnblindedPaymentTokensRequest() = default;

// PUT /v1/confirmation/payment/{payment_id}

std::string RedeemUnblindedPaymentTokensRequest::BuildUrl(
    const WalletInfo& wallet_info) const {
  DCHECK(!wallet_info.payment_id.empty());

  std::string endpoint = "/v1/confirmation/payment/";
  endpoint += wallet_info.payment_id;

  return server::GetDomain().append(endpoint);
}

UrlRequestMethod RedeemUnblindedPaymentTokensRequest::GetMethod() const {
  return UrlRequestMethod::PUT;
}

std::string RedeemUnblindedPaymentTokensRequest::BuildBody(
    const privacy::UnblindedTokenList& unblinded_tokens,
    const std::string& payload) const {
  DCHECK(!payload.empty());

  base::Value dictionary(base::Value::Type::DICTIONARY);

  auto payment_request_dto = CreatePaymentRequestDTO(unblinded_tokens, payload);

  dictionary.SetKey("paymentCredentials", std::move(payment_request_dto));

  dictionary.SetKey("payload", base::Value(payload));

  std::string json;
  base::JSONWriter::Write(dictionary, &json);

  return json;
}

std::string RedeemUnblindedPaymentTokensRequest::CreatePayload(
    const WalletInfo& wallet_info) const {
  DCHECK(!wallet_info.payment_id.empty());

  base::Value payload(base::Value::Type::DICTIONARY);
  payload.SetKey("paymentId", base::Value(wallet_info.payment_id));

  std::string json;
  base::JSONWriter::Write(payload, &json);

  return json;
}

std::vector<std::string>
RedeemUnblindedPaymentTokensRequest::BuildHeaders() const {
  std::string accept_header = "accept: ";
  accept_header += GetAcceptHeaderValue();

  return {
    accept_header
  };
}

std::string RedeemUnblindedPaymentTokensRequest::GetAcceptHeaderValue() const {
  return "application/json";
}

std::string RedeemUnblindedPaymentTokensRequest::GetContentType() const {
  return "application/json";
}

///////////////////////////////////////////////////////////////////////////////

base::Value RedeemUnblindedPaymentTokensRequest::CreatePaymentRequestDTO(
    const privacy::UnblindedTokenList& unblinded_tokens,
    const std::string& payload) const {
  DCHECK(!unblinded_tokens.empty());

  base::Value payment_credentials(base::Value::Type::LIST);

  for (const auto& unblinded_token : unblinded_tokens) {
    base::Value payment_credential(base::Value::Type::DICTIONARY);

    auto credential = CreateCredential(unblinded_token, payload);
    payment_credential.SetKey("credential", base::Value(std::move(credential)));

    payment_credential.SetKey("publicKey",
        base::Value(unblinded_token.public_key));

    payment_credentials.Append(std::move(payment_credential));
  }

  return payment_credentials;
}

base::Value RedeemUnblindedPaymentTokensRequest::CreateCredential(
    const privacy::UnblindedTokenInfo& unblinded_token,
    const std::string& payload) const {
  DCHECK(!payload.empty());

  base::Value credential(base::Value::Type::DICTIONARY);

  auto verification_key = unblinded_token.value.derive_verification_key();
  auto signed_verification_key = verification_key.sign(payload);
  auto signed_verification_key_base64 = signed_verification_key.encode_base64();
  credential.SetKey("signature", base::Value(signed_verification_key_base64));

  auto preimage = unblinded_token.value.preimage();
  auto preimage_base64 = preimage.encode_base64();
  credential.SetKey("t", base::Value(preimage_base64));

  return credential;
}

}  // namespace ads
