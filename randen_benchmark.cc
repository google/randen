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

// Please disable Turbo Boost and CPU throttling!

#include "randen.h"

// std::uniform_*_distribution are slow due to division/log2; we provide
// faster variants if this is 0.
#define USE_STD_DISTRIBUTIONS 0

// Which engines to benchmark.
#define ENABLE_RANDEN 1
#define ENABLE_PCG 1
#define ENABLE_MT 1
#if defined(__SSE2__) && defined(__AES__)
#define ENABLE_CHACHA 1
#else
#define ENABLE_CHACHA 0
#endif
#define ENABLE_OS 1

#if ENABLE_PCG
#include "third_party/pcg_random/include/pcg_random.hpp"
#endif

#if ENABLE_MT
#include <random>
#endif

#if ENABLE_CHACHA
#include "engine_chacha.h"
#endif

#if ENABLE_OS
#include "engine_os.h"
#endif


#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <stdio.h>
#include <algorithm>
#include <numeric>  // iota

#include "nanobenchmark.h"
#include "util.h"

namespace randen {
namespace {

#if USE_STD_DISTRIBUTIONS
using UniformInt = std::uniform_int_distribution<int>;
using UniformDouble = std::uniform_real_distribution<double>;
#else
// These are subsets of std::uniform_*_distribution.

class UniformInt {
 public:
  // (To support u64, add a Multiply overload and GetU64 as below.)
  using result_type = uint32_t;

  struct param_type {
    using distribution_type = UniformInt;

    param_type(const result_type begin, const result_type end)
        : begin(begin), end(end) {}

    // Half-open interval.
    result_type begin;
    result_type end;
  };

  // Engine is a C++11 UniformRandomBitGenerator returning >= 32 bits.
  template <class Engine>
  result_type operator()(Engine& engine, const param_type param) const {
    using Bits = decltype(engine());  // == typename Engine::result_type
    static_assert(std::is_same<uint32_t, Bits>::value ||
                      std::is_same<uint64_t, Bits>::value,
                  "Need u32 or u64");

    // We assume range < pow(2, sizeof(decltype(engine()))*8).
    const result_type range = param.end - param.begin;

    // Division-free with high probability. Algorithm and variable names are
    // from https://arxiv.org/pdf/1805.10941.pdf.
    result_type x = engine();  // (possibly a narrowing conversion from Bits)
    result_type hi, lo;
    Multiply(x, range, &hi, &lo);
    // Rejected, try again (unlikely for small ranges).
    if (lo < range) {
      const result_type t = Negate(range) % range;
      while (hi < t) {
        x = engine();
        Multiply(x, range, &hi, &lo);
      }
    }

    return hi + param.begin;
  }

 private:
  static constexpr result_type Negate(result_type x) {
    return ~x + 1;  // assumes two's complement.
  }

  static void Multiply(const uint32_t x, const uint32_t y, uint32_t* hi,
                       uint32_t* lo) {
    const uint64_t wide = static_cast<uint64_t>(x) * y;
    *hi = wide >> 32;
    *lo = static_cast<uint32_t>(wide & 0xFFFFFFFFu);
  }
};

class UniformDouble {
 public:
  // (Can also be float - we would just cast from double.)
  using result_type = double;

  // Engine is a C++11 UniformRandomBitGenerator returning either u32 or u64.
  template <class Engine>
  result_type operator()(Engine& engine) const {
    uint64_t bits = GetU64(decltype(engine())(), engine);
    if (bits == 0) return static_cast<result_type>(0.0);
    const int leading_zeros = NumZeroBitsAboveMSBNonzero(bits);
    bits <<= leading_zeros;  // shift out leading zeros
    bits >>= (64 - 53);      // zero exponent
    const uint64_t exp = 1022 - leading_zeros;
    const uint64_t ieee = (exp << 52) | bits;
    double ret;
    memcpy(&ret, &ieee, sizeof(ret));
    return static_cast<result_type>(ret);
  }

 private:
  template <class Engine>
  static uint64_t GetU64(uint64_t, Engine& engine) {
    return engine();
  }

  // Adapter for generating u64 from u32 engine.
  template <class Engine>
  static uint64_t GetU64(uint32_t, Engine& engine) {
    uint64_t ret = engine();
    ret <<= 32;
    ret |= engine();
    return ret;
  }
};
#endif  // !USE_STD_DISTRIBUTIONS

// Benchmark::Num64() is passed to its constructor and operator() after
// multiplying with a (non-compile-time-constant) 1 to prevent constant folding.
// It is also used to compute cycles per byte.

// Microbenchmark: generates N numbers in a tight loop.
struct BenchmarkLoop {
  // Large enough that we can ignore size % buffer size.
  static size_t Num64() { return 100000; }

  explicit BenchmarkLoop(const uint64_t num_64) {}

  template <class Engine>
  uint64_t operator()(const uint64_t num_64, Engine& engine) const {
    for (size_t i = 0; i < num_64 - 1; ++i) {
      (void)engine();
    }
    return engine();
  }
};

// Real-world benchmark: shuffles a vector.
class BenchmarkShuffle {
 public:
  static size_t Num64() { return 50000; }

  explicit BenchmarkShuffle(const uint64_t num_64) : ints_to_shuffle_(num_64) {}

  template <class Engine>
  uint64_t operator()(const uint64_t num_64, Engine& engine) const {
    ints_to_shuffle_[0] = static_cast<int>(num_64 & 0xFFFF);
#if USE_STD_DISTRIBUTIONS
    std::shuffle(ints_to_shuffle_.begin(), ints_to_shuffle_.end(), engine);
#else
    // Similar algorithm, but UniformInt instead of std::u_i_d => 2-3x speedup.
    UniformInt dist;
    for (size_t i = num_64 - 1; i != 0; --i) {
      const UniformInt::param_type param(0, i);
      std::swap(ints_to_shuffle_[i], ints_to_shuffle_[dist(engine, param)]);
    }
#endif
    return ints_to_shuffle_[0];
  }

 private:
  mutable std::vector<int> ints_to_shuffle_;
};

// Reservoir sampling.
class BenchmarkSample {
 public:
  static size_t Num64() { return 50000; }

  explicit BenchmarkSample(const uint64_t num_64)
      : population_(num_64), chosen_(kNumChosen) {
    std::iota(population_.begin(), population_.end(), 0);
  }

  template <class Engine>
  uint64_t operator()(const uint64_t num_64, Engine& engine) const {
    // Can replace with std::sample after C++17.
    std::copy(population_.begin(), population_.begin() + kNumChosen,
              chosen_.begin());
    UniformInt dist;
    for (size_t i = kNumChosen; i < num_64; ++i) {
      const UniformInt::param_type param(0, i);
      const size_t index = dist(engine, param);
      if (index < kNumChosen) {
        chosen_[index] = population_[i];
      }
    }

    return chosen_.front();
  }

 private:
  static constexpr size_t kNumChosen = 10000;

  std::vector<int> population_;
  mutable std::vector<int> chosen_;
};

// Actual application: Monte Carlo estimation of Pi * 1E6.
class BenchmarkMonteCarlo {
 public:
  static size_t Num64() { return 200000; }

  explicit BenchmarkMonteCarlo(const uint64_t num_64) {}

  template <class Engine>
  uint64_t operator()(const uint64_t num_64, Engine& engine) const {
    int64_t in_circle = 0;
    for (size_t i = 0; i < num_64; i += 2) {
      const double x = dist_(engine);
      const double y = dist_(engine);
      in_circle += (x * x + y * y) < 1.0;
    }
    return 8 * 1000 * 1000 * in_circle / num_64;
  }

 private:
  mutable UniformDouble dist_;
};

template <class Benchmark, class Engine>
void RunBenchmark(const char* caption, Engine& engine, const int unpredictable1,
                  const Benchmark& benchmark) {
  printf("%8s: ", caption);
  const size_t kNumInputs = 1;
  const FuncInput inputs[kNumInputs] = {
      static_cast<FuncInput>(Benchmark::Num64() * unpredictable1)};
  Result results[kNumInputs];

  Params p;
  p.verbose = false;
#if defined(__powerpc__)
  p.max_evals = 7;
#else
  p.max_evals = 8;
#endif
  p.target_rel_mad = 0.002;
  const size_t num_results = MeasureClosure(
      [&benchmark, &engine](const FuncInput input) {
        return benchmark(input, engine);
      },
      inputs, kNumInputs, results, p);
  RANDEN_CHECK(num_results == kNumInputs);
  for (size_t i = 0; i < num_results; ++i) {
    const double cycles_per_byte =
        results[i].ticks / (results[i].input * sizeof(uint64_t));
    const double mad = results[i].variability * cycles_per_byte;
    printf("%6zu: %5.2f (+/- %5.3f)\n", results[i].input, cycles_per_byte, mad);
  }
}

// Calls RunBenchmark for each (enabled) engine.
template <class Benchmark>
void ForeachEngine(const int unpredictable1) {
  using T = uint64_t;  // WARNING: keep in sync with MT/PCG.

  const Benchmark benchmark(
      static_cast<uint64_t>(Benchmark::Num64() * unpredictable1));

#if ENABLE_RANDEN
  Randen<T> eng_randen;
  RunBenchmark("Randen", eng_randen, unpredictable1, benchmark);
#endif

#if ENABLE_PCG
  // Quoting from pcg_random.hpp: "the c variants offer better crypographic
  // security (just how good the cryptographic security is is an open
  // question)".
  pcg64_c32 eng_pcg;
  RunBenchmark("PCG", eng_pcg, unpredictable1, benchmark);
#endif

#if ENABLE_MT
  std::mt19937_64 eng_mt;
  RunBenchmark("MT", eng_mt, unpredictable1, benchmark);
#endif


#if ENABLE_CHACHA
  ChaCha<T> eng_chacha(0x243f6a8885a308d3ull, 0x243F6A8885A308D3ull);
  RunBenchmark("ChaCha8", eng_chacha, unpredictable1, benchmark);
#endif

#if ENABLE_OS
  EngineOS<T> eng_os;
  RunBenchmark("OS", eng_os, unpredictable1, benchmark);
#endif

  printf("\n");
}

void RunAll(int argc, char* argv[]) {
  // Immediately output any results (for non-local runs).
  setvbuf(stdout, nullptr, _IONBF, 0);

  printf("Config: enable std=%d\n", USE_STD_DISTRIBUTIONS);

  // Avoid migrating between cores - important on multi-socket systems.
  int cpu = -1;
  if (argc == 2) {
    cpu = strtol(argv[1], nullptr, 10);
  }
  platform::PinThreadToCPU(cpu);

  // Ensures the iteration counts are not compile-time constants.
  const int unpredictable1 = argc != 999;

  ForeachEngine<BenchmarkLoop>(unpredictable1);
  ForeachEngine<BenchmarkShuffle>(unpredictable1);
  ForeachEngine<BenchmarkSample>(unpredictable1);
  ForeachEngine<BenchmarkMonteCarlo>(unpredictable1);
}

}  // namespace
}  // namespace randen

int main(int argc, char* argv[]) {
  randen::RunAll(argc, argv);
  return 0;
}
