/*
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * Licensed under the Apache License v2.0 with LLVM Exceptions (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * https://llvm.org/LICENSE.txt
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "amazon-braket-qdmi-device/Trust.hpp"

#include <aws/core/Aws.h>
#include <memory>
#include <string>

#ifdef AMAZON_BRAKET_QDMI_CURL_TRUST
#include <aws/core/client/ClientConfiguration.h>
#include <aws/core/http/HttpClientFactory.h>
#include <aws/core/http/URI.h>
#include <aws/core/http/curl/CurlHttpClient.h>
#include <aws/core/http/standard/StandardHttpRequest.h>
#include <cstdio>
#include <curl/curl.h>
#include <string_view>
#endif

namespace amazon::braket::qdmi::detail {

#ifdef AMAZON_BRAKET_QDMI_CURL_TRUST
class BundleHttpClient : public Aws::Http::CurlHttpClient {
public:
  BundleHttpClient(const Aws::Client::ClientConfiguration& configuration,
                   std::string bundle)
      : CurlHttpClient(configuration), bundle_(std::move(bundle)) {}

protected:
  auto OverrideOptionsOnConnectionHandle(CURL* handle) const -> void override {
    if (curl_easy_setopt(handle, CURLOPT_CAINFO, bundle_.c_str()) != CURLE_OK ||
        curl_easy_setopt(handle, CURLOPT_CAPATH, nullptr) != CURLE_OK ||
        curl_easy_setopt(handle, CURLOPT_SSL_OPTIONS, 0L) != CURLE_OK ||
        curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 1L) != CURLE_OK ||
        curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 2L) != CURLE_OK) {
      // Clearing the URL prevents transmission and preserves the SDK's cleanup.
      curl_easy_reset(handle);
      std::fputs("Could not apply AWS CA bundle; refusing TLS request.\n",
                 stderr);
    }
  }

private:
  std::string bundle_;
};

class BundleHttpFactory : public Aws::Http::HttpClientFactory {
public:
  explicit BundleHttpFactory(std::shared_ptr<TrustPolicy> policy)
      : policy_(std::move(policy)) {}

  auto
  CreateHttpClient(const Aws::Client::ClientConfiguration& configuration) const
      -> std::shared_ptr<Aws::Http::HttpClient> override {
    return std::make_shared<BundleHttpClient>(configuration, policy_->bundle);
  }

  auto CreateHttpRequest(const Aws::String& uri, Aws::Http::HttpMethod method,
                         const Aws::IOStreamFactory& streamFactory) const
      -> std::shared_ptr<Aws::Http::HttpRequest> override {
    return CreateHttpRequest(Aws::Http::URI{uri}, method, streamFactory);
  }

  auto CreateHttpRequest(const Aws::Http::URI& uri,
                         Aws::Http::HttpMethod method,
                         const Aws::IOStreamFactory& streamFactory) const
      -> std::shared_ptr<Aws::Http::HttpRequest> override {
    auto request =
        std::make_shared<Aws::Http::Standard::StandardHttpRequest>(uri, method);
    request->SetResponseStreamFactory(streamFactory);
    return request;
  }

  auto InitStaticState() -> void override {
    Aws::Http::CurlHttpClient::InitGlobalState();
    const auto* version = curl_version_info(CURLVERSION_NOW);
    policy_->httpConfigured =
        version != nullptr && version->ssl_version != nullptr &&
        std::string_view{version->ssl_version}.starts_with("OpenSSL/");
  }

  auto CleanupStaticState() -> void override {
    Aws::Http::CurlHttpClient::CleanupGlobalState();
  }

private:
  std::shared_ptr<TrustPolicy> policy_;
};
#endif

/// Install trust only through callbacks run by the first SDK initialization.
auto configureTrust(Aws::SDKOptions& options,
                    const std::shared_ptr<TrustPolicy>& policy) -> bool {
  options.httpOptions.httpClientFactory_create_fn = {};
  options.ioOptions.tlsConnectionOptions_create_fn = {};
  if (policy->bundle.empty()) {
    return true;
  }
#ifdef AMAZON_BRAKET_QDMI_CURL_TRUST
  const auto factory = std::make_shared<BundleHttpFactory>(policy);
  options.httpOptions.httpClientFactory_create_fn = [factory] {
    return factory;
  };
  options.ioOptions.tlsConnectionOptions_create_fn = [policy] {
    using namespace Aws::Crt::Io;
    try {
      auto contextOptions = TlsContextOptions::InitDefaultClient();
      if (!contextOptions.OverrideDefaultTrustStore(nullptr,
                                                    policy->bundle.c_str())) {
        return std::shared_ptr<TlsConnectionOptions>{};
      }
      TlsContext context(contextOptions, TlsMode::CLIENT);
      if (!context) {
        return std::shared_ptr<TlsConnectionOptions>{};
      }
      auto connections = std::make_shared<TlsConnectionOptions>(
          context.NewConnectionOptions());
      policy->crtConfigured = static_cast<bool>(*connections);
      return connections;
    } catch (...) {
      // Finish SDK initialization so the caller can balance ShutdownAPI safely.
      return std::shared_ptr<TlsConnectionOptions>{};
    }
  };
  return true;
#else
  return false;
#endif
}

} // namespace amazon::braket::qdmi::detail
