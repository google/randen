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
#include <unistd.h>     // sleep
#include <wmmintrin.h>  // AES

#include "nanobenchmark.h"
#include "randen.h"

#define ASSERT_TRUE(condition)                     \
  while (!(condition)) {                           \
    printf("Check failed at line %d\n", __LINE__); \
    abort();                                       \
  }

namespace nanobenchmark {
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
  Result results[N];
  Params params;
  params.max_evals = 4;  // avoid test timeout
  const size_t num_results = Measure(&AES, nullptr, inputs, N, results, params);
  for (size_t i = 0; i < num_results; ++i) {
    printf("%5zu: %6.2f ticks; MAD=%4.2f%%\n", results[i].input,
           results[i].ticks, results[i].variability * 100.0);
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
  Result results[N];
  Params params;
  params.max_evals = 4;  // avoid test timeout
  const size_t num_results = Measure(&Div, nullptr, inputs, N, results, params);
  for (size_t i = 0; i < num_results; ++i) {
    printf("%5zu: %6.2f ticks; MAD=%4.2f%%\n", results[i].input,
           results[i].ticks, results[i].variability * 100.0);
  }
}

randen::Randen<uint32_t> rng;

// A function whose runtime depends on rng.
uint64_t Random(const void* arg, FuncInput in) {
  const uint32_t r = rng() & 0xF;
  return AES(arg, r * r);
}

// Ensure the measured variability is high.
template <size_t N>
void MeasureRandom(const FuncInput (&inputs)[N]) {
  Result results[N];
  Params p;
  p.max_evals = 4;  // avoid test timeout
  p.verbose = false;
  const size_t num_results = Measure(&Random, nullptr, inputs, N, results, p);
  for (size_t i = 0; i < num_results; ++i) {
    ASSERT_TRUE(results[i].variability > 1E-3);
  }
}

template <size_t N>
void EnsureLongMeasurementFails(const FuncInput (&inputs)[N]) {
  printf("Expect a 'measurement failed' below:\n");
  Result results[N];
  const size_t num_results = MeasureClosure(
      [](const FuncInput input) {
        // Loop until the sleep succeeds (not interrupted by signal). We assume
        // >= 512 MHz, so 2 seconds will exceed the 1 << 30 tick safety limit.
        while (sleep(2) != 0) {
        }
        return input;
      },
      inputs, N, results);
  ASSERT_TRUE(num_results == 0);
}

void RunAll(const int argc, char* argv[]) {
  // Avoid migrating between cores - important on multi-socket systems.
  int cpu = -1;
  if (argc == 2) {
    cpu = strtol(argv[1], nullptr, 10);
  }
  platform::PinThreadToCPU(cpu);

  // unpredictable == 1 but the compiler doesn't know that.
  const int unpredictable = argc != 999;
  static const FuncInput inputs[] = {static_cast<FuncInput>(unpredictable) + 2,
                                     static_cast<FuncInput>(unpredictable + 9)};

  MeasureAES(inputs);
  MeasureDiv(inputs);
  MeasureRandom(inputs);
  EnsureLongMeasurementFails(inputs);
}

}  // namespace
}  // namespace nanobenchmark

int main(int argc, char* argv[]) {
  nanobenchmark::RunAll(argc, argv);
  return 0;
}
