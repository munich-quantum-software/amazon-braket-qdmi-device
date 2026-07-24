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

#pragma once

#include <slurm/slurm_errno.h>

using spank_t = struct spank_handle*;
using spank_err_t = slurm_err_t;

// These declarations intentionally mirror Slurm's public SPANK ABI.
// NOLINTBEGIN(readability-identifier-naming, performance-enum-size,
//             modernize-use-scoped-enum)
enum spank_context {
  S_CTX_LOCAL = 0,
  S_CTX_REMOTE = 1,
  S_CTX_ALLOCATOR = 2,
};
using spank_context_t = enum spank_context;

using spank_opt_cb_f = int (*)(int, const char*, int);

struct spank_option {
  char* name;
  char* arginfo;
  char* usage;
  int has_arg;
  int val;
  spank_opt_cb_f cb;
};

#define SPANK_PLUGIN(name, version)

extern "C" {
int spank_remote(spank_t spank);
spank_context_t spank_context(void);
spank_err_t spank_option_register(spank_t spank, spank_option* option);
spank_err_t spank_option_getopt(spank_t spank, spank_option* option,
                                char** argument);
spank_err_t spank_getenv(spank_t spank, const char* name, char* buffer,
                         int length);
spank_err_t spank_setenv(spank_t spank, const char* name, const char* value,
                         int overwrite);
void slurm_spank_log(const char* format, ...);
}

// NOLINTEND(readability-identifier-naming, performance-enum-size,
//           modernize-use-scoped-enum)
