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

// Please first run `blaze shutdown` and disable Turbo Boost and CPU throttling!

#include "third_party/randen/randen.h"

#include <stdio.h>
#include <algorithm>
#include <random>
#include "third_party/pcg_random/include/pcg_random.hpp"
#include "third_party/randen/nanobenchmark.h"

// std::uniform_real_distribution is slow (x87 instructions) and non-uniform.
// When 0, we use a custom implementation instead.
#define ENABLE_STD_UNIFORM_REAL 0

namespace randen {
namespace {

#define ASSERT_TRUE(condition)                           \
  do {                                                   \
    if (!(condition)) {                                  \
      printf("Assertion failed on line %d\n", __LINE__); \
      abort();                                           \
    }                                                    \
  } while (false)

// Microbenchmark: generates N numbers in a tight loop.
struct BenchmarkLoop {
  // Large enough that we can ignore size % buffer size.
  static constexpr int kNum64 = 100000;

  template <class Engine>
  uint64_t operator()(const uint64_t num_iterations, Engine& engine) const {
    for (size_t i = 0; i < num_iterations - 1; ++i) {
      (void)engine();
    }
    return engine();
  }
};

// Real-world benchmark: shuffles a vector.
class BenchmarkShuffle {
 public:
  static constexpr int kNum64 = 50000;

  BenchmarkShuffle() : ints_to_shuffle_(kNum64) {}

  template <class Engine>
  uint64_t operator()(const uint64_t input, Engine& engine) const {
    ints_to_shuffle_[0] = static_cast<int>(input & 0xFFFF);
    std::shuffle(ints_to_shuffle_.begin(), ints_to_shuffle_.end(), engine);
    return ints_to_shuffle_[0];
  }

 private:
  mutable std::vector<int> ints_to_shuffle_;
};

// Reservoir sampling.
class BenchmarkSample {
 public:
  static constexpr int kNum64 = 50000;

  BenchmarkSample() : population_(kNum64), chosen_(kNumChosen) {
    std::iota(population_.begin(), population_.end(), 0);
  }

  template <class Engine>
  uint64_t operator()(const uint64_t num_64, Engine& engine) const {
    // Can replace with std::sample after C++17.
    std::copy(population_.begin(), population_.begin() + kNumChosen,
              chosen_.begin());
    for (int i = kNumChosen; i < kNum64; ++i) {
      std::uniform_int_distribution<int> dist(0, i);  // closed interval
      const int index = dist(engine);
      if (index < kNumChosen) {
        chosen_[index] = population_[i];
      }
    }

    return chosen_.front();
  }

 private:
  static constexpr int kNumChosen = 10000;

  std::vector<int> population_;
  mutable std::vector<int> chosen_;
};

// Actual application: Monte Carlo estimation of Pi * 1E6.
class BenchmarkMonteCarlo {
 public:
  static constexpr int kNum64 = 200000;

  template <class Engine>
  uint64_t operator()(const uint64_t num_64, Engine& engine) const {
    int in_circle = 0;
    for (size_t i = 0; i < num_64; i += 2) {
      const double x = Uniform01(engine);
      const double y = Uniform01(engine);
      in_circle += (x * x + y * y) < 1.0;
    }
    return 8E6 * in_circle / num_64;
  }

 private:
  template <class Engine>
  double Uniform01(Engine& engine) const {
#if ENABLE_STD_UNIFORM_REAL
    return dist_(engine);
#else
    uint64_t bits = engine();
    if (bits == 0) return 0.0;
    const int leading_zeros = __builtin_clzll(bits);
    bits <<= leading_zeros;  // shift out leading zeros
    bits >>= (64 - 53);      // zero exponent
    const uint64_t exp = 1022 - leading_zeros;
    const uint64_t ieee = (exp << 52) | bits;
    double ret;
    memcpy(&ret, &ieee, sizeof(ret));
    return ret;
#endif
  }

#if ENABLE_STD_UNIFORM_REAL
  mutable std::uniform_real_distribution<> dist_;
#endif
};

template <class Benchmark, class Engine>
void RunBenchmark(const char* caption, const Benchmark& benchmark,
                  Engine& engine) {
  printf("%6s: ", caption);
  const size_t kNumInputs = 1;
  const FuncInput inputs[kNumInputs] = {Benchmark::kNum64};

  Params p;
  p.verbose = false;
  p.target_rel_mad = 0.002;
  const auto& ret = NB_NAMESPACE::MeasureClosure(
      [&benchmark, &engine](const FuncInput input) {
        return benchmark(input, engine);
      },
      inputs, kNumInputs, p);
  ASSERT_TRUE(ret.num_results == kNumInputs);
  const Result& r = ret.results[0];
  const double mul = 1.0 / (r.input * sizeof(uint64_t));
  printf("%6zu: %4.2f ticks; MAD=%4.2f%%\n", r.input, r.ticks * mul,
         r.variability * 100.0);
}

template <class Benchmark>
void ForeachEngine(const Benchmark& benchmark) {
  // Random 'engines' under consideration (all 64-bit):
  Randen<uint64_t> eng_randen;
  // Quoting from pcg_random.hpp: "the c variants offer better crypographic
  // security (just how good the cryptographic security is is an open
  // question)".
  pcg64_c32 eng_pcg;
  std::mt19937_64 eng_mt;

  RunBenchmark("Randen", benchmark, eng_randen);
  RunBenchmark("PCG", benchmark, eng_pcg);
  RunBenchmark("MT", benchmark, eng_mt);
}

void RunAll() {
  // Immediately output any results (for non-local runs).
  setvbuf(stdout, nullptr, _IONBF, 0);

  ForeachEngine(BenchmarkLoop());
  ForeachEngine(BenchmarkShuffle());
  ForeachEngine(BenchmarkSample());
  ForeachEngine(BenchmarkMonteCarlo());
}

}  // namespace
}  // namespace randen

int main(int argc, char* argv[]) {
  randen::RunAll();
  return 0;
}
