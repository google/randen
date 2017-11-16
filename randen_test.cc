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

#include "third_party/randen/randen.h"

#include <stdio.h>
#include <algorithm>
#include <random>  // seed_seq

#define UPDATE_GOLDEN 0
#define ENABLE_VERIFY 1
#define ENABLE_DUMP 0

namespace randen {
namespace {

#define ASSERT_TRUE(condition)                           \
  do {                                                   \
    if (!(condition)) {                                  \
      printf("Assertion failed on line %d\n", __LINE__); \
      abort();                                           \
    }                                                    \
  } while (false)

using EngRanden = Randen<uint64_t>;

#if ENABLE_VERIFY

void VerifyReseedChangesAllValues() {
  const size_t kNumOutputs = 127;
  EngRanden engine;

  std::seed_seq seq1{1, 2, 3, 4, 5, 6, 7};
  engine.seed(seq1);
  uint64_t out1[kNumOutputs];
  for (size_t i = 0; i < kNumOutputs; ++i) {
    out1[i] = engine();
  }

  std::seed_seq seq2{127, 255, 511};
  engine.seed(seq2);
  uint64_t out2[kNumOutputs];
  engine.seed(seq2);

  for (size_t i = 0; i < kNumOutputs; ++i) {
    out2[i] = engine();
    ASSERT_TRUE(out2[i] != out1[i]);
  }
}

void VerifyDiscard() {
  const int N = 56;  // two buffer's worth
  for (int num_used = 0; num_used < N; ++num_used) {
    EngRanden engine_used;
    for (int i = 0; i < num_used; ++i) {
      (void)engine_used();
    }

    for (int num_discard = 0; num_discard < N; ++num_discard) {
      EngRanden engine1 = engine_used;
      EngRanden engine2 = engine_used;
      for (int i = 0; i < num_discard; ++i) {
        (void)engine1();
      }
      engine2.discard(num_discard);
      for (int i = 0; i < N; ++i) {
        const uint64_t r1 = engine1();
        const uint64_t r2 = engine2();
        ASSERT_TRUE(r1 == r2);
      }
    }
  }
}

void VerifyGolden() {
  // prime number => some buffer values unused.
  const size_t kNumOutputs = 127;
#if UPDATE_GOLDEN
  EngRanden engine;
  for (size_t i = 0; i < kNumOutputs; ++i) {
    printf("0x%016lx,\n", engine());
  }
  printf("\n");
#else
  const uint64_t golden[kNumOutputs] = {
      0xdda9f47cd90410ee, 0xc3c14f134e433977, 0xf0b780f545c72912,
      0x887bf3087fd8ca10, 0x30ec63baff3c6d59, 0x15dbb1d37696599f,
      0x02808a316f49a54c, 0xb29f73606f7f20a6, 0x9cbf605e3fd9de8a,
      0x3b8feaf9d5c8e50e, 0xd8b2ffd356301ed5, 0xc970ae1a78183bbb,
      0xcdfd8d76eb8f9a19, 0xf4b327fe0fc73c37, 0xd5af05dd3eff9556,
      0xc3a506eb91420c9d, 0x7023920e0d6bfe8c, 0x48db1bb78f83c4a1,
      0xed1ef4c26b87b840, 0x58d3575834956d42, 0x497cabf3431154fc,
      0x8eef32a23e0b2df3, 0xd88b5749f090e5ea, 0x4e24370570029a8b,
      0x78fcec2cbb6342f5, 0xc651a582a970692f, 0x352ee4ad1816afe3,
      0x463cb745612f55db, 0x811ef0821c3de851, 0x026ff374c101da7e,
      0xa0660379992d58fc, 0x6f7e616704c4fa59, 0x915f3445685da798,
      0x04b0a374a3b795c7, 0x4663352533ce1882, 0x26802a8ac76571ce,
      0x5588ba3a4d6e6c51, 0xb9fdefb4a24dc738, 0x607195a5e200f5fd,
      0xa2101a42d35f1956, 0xe1e5e03c759c0709, 0x7e100308f3290764,
      0xcbcf585399e432f1, 0x082572cc5da6606f, 0x0904469acbfee8f2,
      0xe8a2be4f8335d8f1, 0x08e8a1f1a69da69a, 0xf08bd31b6daecd51,
      0x2e9705bb053d6b46, 0x6542a20aad57bff5, 0x78e3a810213b6ffb,
      0xda2fc9db0713c391, 0xc0932718cd55781f, 0xdc16a59cdd85f8a6,
      0xb97289c1be0f2f9c, 0xb9bfb29c2b20bfe5, 0x5524bb834771435b,
      0xc0a2a0e403a892d4, 0xff4af3ab8d1b78c5, 0x8265da3d39d1a750,
      0x66e455f627495189, 0xf0ec5f424bcad77f, 0x3424e47dc22596e3,
      0xc82d3120b57e3270, 0xc191c595afc4dcbf, 0xbc0c95129ccedcdd,
      0x7f90650ea6cd6ab4, 0x120392bd2bb70939, 0xa7c8fac5a7917eb0,
      0x7287491832695ad3, 0x7c1bf9839c7c1ce5, 0xd088cb9418be0361,
      0x78565cdefd28c4ad, 0xe2e991fa58e1e79e, 0x2a9eac28b08c96bf,
      0x7351b9fef98bafad, 0x13a685861bab87e0, 0x6c4f179696cb2225,
      0x30537425cac70991, 0x64c6de5aa0501971, 0x7e05e3aa8ec720dc,
      0x01590d9dc6c532b7, 0x738184388f3bc1d2, 0x74a07d9c54e3e63f,
      0x6bcdf185561f255f, 0x26ffdc5067be3acb, 0x171df81934f68604,
      0xa0eaf2e1cf99b1c6, 0x5d1cb02075ba1cea, 0x7ea5a21665683e5a,
      0xba6364eff80de02f, 0x957f38cbd2123fdf, 0x892d8317de82f7a2,
      0x606e0a0e41d452ee, 0x4eb28826766fcf5b, 0xe707b1db50f7b43e,
      0x6ee217df16527d78, 0x5a362d56e80a0951, 0x443e63857d4076ca,
      0xf6737962ba6b23dd, 0xd796b052151ee94d, 0x790d9a5f048adfeb,
      0x8b833ff84893da5d, 0x033ed95c12b04a03, 0x9877c4225061ca76,
      0x3d6724b1bb15eab9, 0x42e5352fe30ce989, 0xd68d6810adf74fb3,
      0x3cdbf7e358df4b8b, 0x265b565a7431fde7, 0x52d2242f65b37f88,
      0x2922a47f6d3e8779, 0x29d40f00566d5e26, 0x5d836d6e2958d6b5,
      0x6c056608b7d9c1b6, 0x288db0e1124b14a0, 0x8fb946504faa6c9d,
      0x0b9471bdb8f19d32, 0xfd1fe27d144a09e0, 0x8943a9464540251c,
      0x8048f217633fce36, 0xea6ac458da141bda, 0x4334b8b02ff7612f,
      0xfeda1384ade74d31, 0x096d119a3605c85b, 0xdbc8441f5227e216,
      0x541ad7efa6ddc1d3};
  EngRanden engine;
  for (size_t i = 0; i < kNumOutputs; ++i) {
    ASSERT_TRUE(golden[i] == engine());
  }
#endif
}

#endif  // ENABLE_VERIFY

void Verify() {
#if ENABLE_VERIFY
  VerifyReseedChangesAllValues();
  VerifyDiscard();
  VerifyGolden();
#endif
}

void DumpOutput() {
#if ENABLE_DUMP
  const size_t kNumOutputs = 1500 * 1000 * 1000;
  std::vector<uint64_t> outputs(kNumOutputs);
  EngRanden engine;
  for (size_t i = 0; i < kNumOutputs; ++i) {
    outputs[i] = engine();
  }

  FILE* f = fopen("/tmp/randen.bin", "wb");
  if (f != nullptr) {
    fwrite(outputs.data(), kNumOutputs, 8, f);
    fclose(f);
  }
#endif  // ENABLE_DUMP
}

void RunAll() {
  // Immediately output any results (for non-local runs).
  setvbuf(stdout, nullptr, _IONBF, 0);

  Verify();
  DumpOutput();
}

}  // namespace
}  // namespace randen

int main(int argc, char* argv[]) {
  randen::RunAll();
  return 0;
}
