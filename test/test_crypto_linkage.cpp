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

#include <curl/curl.h>
#include <dlfcn.h>
#include <gtest/gtest.h>
#include <memory>

TEST(AmazonBraketQDMICryptoLinkageTest, InitializesWithSystemCurlLoaded) {
  ASSERT_NE(curl_version(), nullptr);
  const std::unique_ptr<void, int (*)(void*)> library(
      dlopen(AMAZON_BRAKET_QDMI_LIBRARY_PATH, RTLD_NOW | RTLD_LOCAL), dlclose);
  ASSERT_NE(library.get(), nullptr) << dlerror();

  using LifecycleFunction = int (*)();
  const auto initialize = reinterpret_cast<LifecycleFunction>(
      dlsym(library.get(), "AMAZON_BRAKET_QDMI_device_initialize"));
  ASSERT_NE(initialize, nullptr) << dlerror();
  const auto finalize = reinterpret_cast<LifecycleFunction>(
      dlsym(library.get(), "AMAZON_BRAKET_QDMI_device_finalize"));
  ASSERT_NE(finalize, nullptr) << dlerror();

  ASSERT_EQ(initialize(), 0);
  EXPECT_EQ(finalize(), 0);
}
