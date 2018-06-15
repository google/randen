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

#ifndef ENGINE_OS_H_
#define ENGINE_OS_H_

#ifdef _WIN64
#define NOMINMAX
#include <windows.h>
// Must come after windows.h; this comment ensures that.
#include <bcrypt.h>
#pragma comment(lib, "bcrypt")
#endif

#include "util.h"

namespace randen {

// Buffered, uses OS CSPRNG.
template <typename T>
class alignas(32) EngineOS {
 public:
  // C++11 URBG interface:
  using result_type = T;
  static constexpr T min() { return T(0); }
  static constexpr T max() { return ~T(0); }

  EngineOS() {
    // The first call to operator() will trigger a refill.
    next_ = kStateT;

#ifdef _WIN32
    RANDEN_CHECK(0 == BCryptOpenAlgorithmProvider(
                          &provider_, BCRYPT_RNG_ALGORITHM, nullptr, 0));
#else
    dev_ = fopen("/dev/urandom", "r");
    RANDEN_CHECK(dev_ != nullptr);
#endif
  }

  ~EngineOS() {
#ifdef _WIN32
    RANDEN_CHECK(0 == BCryptCloseAlgorithmProvider(provider_, 0));
#else
    RANDEN_CHECK(fclose(dev_) == 0);
#endif
  }

  // Returns random bits from the buffer in units of T.
  T operator()() {
    // (Local copy ensures compiler knows this is not aliased.)
    size_t next = next_;

    // Refill the buffer if needed (unlikely).
    if (next >= kStateT) {
#ifdef _WIN32
      RANDEN_CHECK(0 == BCryptGenRandom(provider_,
                                        reinterpret_cast<BYTE*>(&state_[0]),
                                        sizeof(state_), 0));
#else
      const size_t bytes_read = fread(&state_[0], 1, sizeof(state_), dev_);
      RANDEN_CHECK(bytes_read == sizeof(state_));
#endif
      next = 0;
    }

    const T ret = state_[next];
    next_ = next + 1;
    return ret;
  }

 private:
  static constexpr size_t kStateT = 256 / sizeof(T);  // same as Randen

  alignas(32) T state_[kStateT];
  size_t next_;  // index within state_
#ifdef _WIN32
  BCRYPT_ALG_HANDLE provider_;
#else
  FILE* dev_;
#endif
};

}  // namespace randen

#endif  // ENGINE_OS_H_
