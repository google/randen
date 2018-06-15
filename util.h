// Copyright 2018 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef UTIL_H_
#define UTIL_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _MSC_VER
#include <intrin.h>
#endif

#define RANDEN_CHECK(condition)                          \
  do {                                                   \
    if (!(condition)) {                                  \
      printf("Assertion failed on line %d\n", __LINE__); \
      abort();                                           \
    }                                                    \
  } while (false)

namespace randen {

// "x" != 0.
static inline int NumZeroBitsAboveMSBNonzero(const uint64_t x) {
#ifdef _MSC_VER
  return static_cast<int>(__lzcnt64(x));  // WARNING: requires BMI2
#else
  return __builtin_clzll(x);
#endif
}

}  // namespace randen

#endif  // UTIL_H_
