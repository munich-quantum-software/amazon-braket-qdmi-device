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

#include <cstdarg>

typedef struct spank_context* spank_t;

enum spank_err {
  ESPANK_SUCCESS = 0,
  ESPANK_ERROR = 1,
};

enum spank_context_type {
  S_CTX_LOCAL = 0,
  S_CTX_REMOTE = 1,
  S_CTX_ALLOCATOR = 2,
};

typedef int (*spank_option_cb_f)(int, const char*, int);

struct spank_option {
  char* name;
  char* arginfo;
  char* usage;
  int has_arg;
  int val;
  spank_option_cb_f cb;
};

#define SPANK_PLUGIN(name, version)

extern "C" {
int spank_remote(spank_t spank);
spank_context_type spank_context(void);
int spank_option_register(spank_t spank, spank_option* option);
int spank_option_getopt(spank_t spank, spank_option* option, char** argument);
int spank_setenv(spank_t spank, const char* name, const char* value,
                 int overwrite);
int slurm_spank_log(spank_t spank, int level, const char* format, ...);
}
