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

#include <stdio.h>
#include <unistd.h>     // sleep

#include "nanobenchmark.h"
#include "randen.h"
#include "util.h"
#include "vector128.h"

namespace randen {
namespace {

uint64_t AES(const void*, const FuncInput num_rounds) {
  // Ensures multiple invocations are serially dependent, otherwise we're
  // measuring the throughput rather than latency.
  static V prev;
  V m = prev;
  for (size_t i = 0; i < num_rounds; ++i) {
    m = AES(m, m);
  }
  prev = m;
  alignas(16) uint64_t lanes[2];
  Store(m, lanes, 0);
  return lanes[0];
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

Randen<uint32_t> rng;

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
    RANDEN_CHECK(results[i].variability > 1E-3);
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
  RANDEN_CHECK(num_results == 0);
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
}  // namespace randen

int main(int argc, char* argv[]) {
  randen::RunAll(argc, argv);
  return 0;
}
