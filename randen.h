// Copyright 2017 Google Inc. All Rights Reserved.
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

// 'Strong' (well-distributed, unpredictable, backtracking-resistant) random
// generator, faster in some benchmarks than std::mt19937_64 and pcg64_c32.

#ifndef RANDEN_H_
#define RANDEN_H_

// RANDen = RANDom generator or beetroots in Swiss German.
namespace randen {

struct Internal {
  static void Absorb(const void* seed, void* state);
  static void Generate(void* state);

  static constexpr int kStateBytes = 256;  // 2048-bit

  // Size of the 'inner' (inaccessible) part of the sponge. Larger values would
  // require more frequent calls to Generate.
  static constexpr int kCapacityBytes = 16;  // 128-bit
};

// Deterministic pseudorandom byte generator with backtracking resistance
// (leaking the state does not compromise prior outputs). Based on Reverie
// (see "A Robust and Sponge-Like PRNG with Improved Efficiency") instantiated
// with an improved Simpira-like permutation.
// Returns values of type "T" (must be a built-in unsigned integer type).
template <typename T>
class alignas(32) Randen {
  using U64 = unsigned long long;

 public:
  // C++11 URBG interface:
  using result_type = T;
  static constexpr T min() { return T(0); }
  static constexpr T max() { return ~T(0); }

  Randen() {
    // The first call to operator() will trigger Generate.
    next_ = kStateT;

    // Cheap state initialization in case operator() is never called.
    // seed() can be called at any time to insert entropy.
    for (U64 i = 0; i < kStateT; ++i) {
      state_[i] = 0;
    }
  }

  // Returns random bits from the buffer in units of T.
  T operator()() {
    // (Local copy ensures compiler knows this is not aliased.)
    U64 next = next_;

    // Refill the buffer if needed (unlikely).
    if (next >= kStateT) {
      Internal::Generate(state_);
      next = kCapacityT;
    }

    const T ret = state_[next];
    next_ = next + 1;
    return ret;
  }

  // Inserts entropy into (part of) the state. Calling this periodically with
  // sufficient entropy ensures prediction resistance (attackers cannot predict
  // future outputs even if state is compromised).
  template <class SeedSequence>
  void seed(SeedSequence& seq) {
    using U32 = typename SeedSequence::result_type;
    constexpr int kRate32 =
        (Internal::kStateBytes - Internal::kCapacityBytes) / sizeof(U32);
    U32 buffer[kRate32];
    seq.generate(buffer, buffer + kRate32);
    Internal::Absorb(buffer, state_);

    Internal::Generate(state_);
    next_ = kCapacityT;
  }

  void discard(U64 count) {
    const U64 remaining = kStateT - next_;
    if (count <= remaining) {
      next_ += count;
      return;
    }
    count -= remaining;

    const U64 kRateT = kStateT - kCapacityT;
    while (count > kRateT) {
      Internal::Generate(state_);
      next_ = kCapacityT;
      count -= kRateT;
    }

    if (count != 0) {
      Internal::Generate(state_);
      next_ = kCapacityT + count;
    }
  }

 private:
  static constexpr U64 kStateT = Internal::kStateBytes / sizeof(T);
  static constexpr U64 kCapacityT = Internal::kCapacityBytes / sizeof(T);

  // First kCapacityT are `inner', the others are accessible random bits.
  alignas(32) T state_[kStateT];
  U64 next_;  // index within state_
};

}  // namespace randen

#endif  // RANDEN_H_
