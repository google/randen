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

#include <smmintrin.h>  // SSE4
#include <stdio.h>
#include <stdlib.h>     // abort
#include <wmmintrin.h>  // AES

#include "third_party/randen/nanobenchmark.h"
#include "third_party/randen/randen.h"

#define ASSERT_TRUE(condition)                     \
  while (!(condition)) {                           \
    printf("Check failed at line %d\n", __LINE__); \
    abort();                                       \
  }

namespace randen {
namespace {

uint64_t AES(const void*, const FuncInput num_rounds) {
  // Ensures multiple invocations are serially dependent, otherwise we're
  // measuring the throughput rather than latency.
  static __m128i prev;
  __m128i m = prev;
  for (size_t i = 0; i < num_rounds; ++i) {
    m = _mm_aesenc_si128(m, m);
  }
  prev = m;
  return _mm_cvtsi128_si64(m);
}

template <size_t N>
void MeasureAES(const FuncInput (&inputs)[N]) {
  const auto& ret = Measure(&AES, nullptr, inputs, N);
  for (size_t i = 0; i < ret.num_results; ++i) {
    const Result& r = ret.results[i];
    printf("%5zu: %6.2f ticks; MAD=%4.2f%%\n", r.input, r.ticks,
           r.variability * 100.0);
    ASSERT_TRUE(r.variability < 1E-4);
  }
}

uint64_t Div(const void*, FuncInput in) {
  // Here we're measuring the throughput because benchmark invocations are
  // independent.
  const int64_t d1 = 0xFFFFFFFFFFll / int64_t(in);  // IDIV
  return d1;
}

template <size_t N>
void MeasureDiv(const FuncInput (&inputs)[N]) {
  const auto& ret = Measure(&Div, nullptr, inputs, N);
  for (size_t i = 0; i < ret.num_results; ++i) {
    const Result& r = ret.results[i];
    printf("%5zu: %6.2f ticks; MAD=%4.2f%%\n", r.input, r.ticks,
           r.variability * 100.0);
    ASSERT_TRUE(r.variability < 1E-4);
  }
}

Randen<uint32_t> rng;

// A function whose runtime depends on rng.
uint64_t Random(const void* arg, FuncInput in) {
  const uint32_t r = rng() & 0xF;
  return AES(arg, r * r);
}

// Ensure the measured variability is high.
template <size_t N>
void MeasureRandom(const FuncInput (&inputs)[N]) {
  Params p;
  p.max_evals = 4;
  p.verbose = false;
  const auto& ret = Measure(&Random, nullptr, inputs, N, p);
  for (size_t i = 0; i < ret.num_results; ++i) {
    const Result& r = ret.results[i];
    ASSERT_TRUE(r.variability > 1E-3);
  }
}

void RunAll(const int unpredictable) {
  // unpredictable == 1 but the compiler doesn't know that.
  static const FuncInput inputs[] = {static_cast<FuncInput>(unpredictable) + 2,
                                     static_cast<FuncInput>(unpredictable + 9)};

  MeasureAES(inputs);
  MeasureDiv(inputs);
  MeasureRandom(inputs);
}

}  // namespace
}  // namespace randen

int main(int argc, char* argv[]) {
  randen::RunAll(argc);
  return 0;
}
