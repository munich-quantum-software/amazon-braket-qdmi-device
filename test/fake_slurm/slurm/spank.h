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
