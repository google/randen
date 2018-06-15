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

#ifndef ENGINE_CHACHA_H_
#define ENGINE_CHACHA_H_

#include <cstdint>
#include <limits>
#include "tmmintrin.h"

namespace randen {

// Modified from https://gist.github.com/orlp/32f5d1b631ab092608b1:
/*
    Copyright (c) 2015 Orson Peters <orsonpeters@gmail.com>

    This software is provided 'as-is', without any express or implied warranty.
   In no event will the authors be held liable for any damages arising from the
   use of this software.

    Permission is granted to anyone to use this software for any purpose,
   including commercial applications, and to alter it and redistribute it
   freely, subject to the following restrictions:

    1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software in a
   product, an acknowledgment in the product documentation would be appreciated
   but is not required.

    2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

    3. This notice may not be removed or altered from any source distribution.
*/

template <typename T>
class ChaCha {
 public:
  static constexpr size_t R = 8;
  typedef T result_type;

  static constexpr result_type min() {
    return std::numeric_limits<result_type>::min();
  }
  static constexpr result_type max() {
    return std::numeric_limits<result_type>::max();
  }

  explicit ChaCha(uint64_t seedval, uint64_t stream = 0) {
    seed(seedval, stream);
  }
  template <class Sseq>
  explicit ChaCha(Sseq& seq) {
    seed(seq);
  }

  void seed(uint64_t seedval, uint64_t stream = 0) {
    ctr = 0;
    keysetup[0] = seedval & 0xffffffffu;
    keysetup[1] = seedval >> 32;
    keysetup[2] = keysetup[3] = 0xdeadbeef;  // Could use 128-bit seed.
    keysetup[4] = stream & 0xffffffffu;
    keysetup[5] = stream >> 32;
    keysetup[6] = keysetup[7] = 0xdeadbeef;  // Could use 128-bit stream.
  }

  template <class Sseq>
  void seed(Sseq& seq) {
    ctr = 0;
    seq.generate(keysetup, keysetup + 8);
  }

  result_type operator()() {
    int idx = ctr % 16;
    if (idx == 0) generate_block();

    result_type ret;
    memcpy(&ret, block + idx, sizeof(ret));
    ctr += sizeof(ret) / sizeof(uint32_t);

    return ret;
  }

 private:
  void generate_block() {
    uint32_t constants[4] = {0x61707865, 0x3320646e, 0x79622d32, 0x6b206574};

    uint32_t input[16];
    for (int i = 0; i < 4; ++i) input[i] = constants[i];
    for (int i = 0; i < 8; ++i) input[4 + i] = keysetup[i];
    input[12] = (ctr / 16) & 0xffffffffu;
    input[13] = (ctr / 16) >> 32;
    input[14] = input[15] = 0xdeadbeef;  // Could use 128-bit counter.

    for (int i = 0; i < 16; ++i) block[i] = input[i];
    chacha_core();
    for (int i = 0; i < 16; ++i) block[i] += input[i];
  }

  // Get an efficient _mm_roti_epi32 based on enabled features.
#define _mm_roti_epi32(r, c)                                                   \
  (((c) == 8)                                                                  \
       ? _mm_shuffle_epi8((r), _mm_set_epi8(14, 13, 12, 15, 10, 9, 8, 11, 6,   \
                                            5, 4, 7, 2, 1, 0, 3))              \
       : ((c) == 16)                                                           \
             ? _mm_shuffle_epi8((r), _mm_set_epi8(13, 12, 15, 14, 9, 8, 11,    \
                                                  10, 5, 4, 7, 6, 1, 0, 3, 2)) \
             : ((c) == 24) ? _mm_shuffle_epi8(                                 \
                                 (r), _mm_set_epi8(12, 15, 14, 13, 8, 11, 10,  \
                                                   9, 4, 7, 6, 5, 0, 3, 2, 1)) \
                           : _mm_xor_si128(_mm_slli_epi32((r), (c)),           \
                                           _mm_srli_epi32((r), 32 - (c))))

  void chacha_core() {
// ROTVn rotates the elements in the given vector n places to the left.
#define CHACHA_ROTV1(x) _mm_shuffle_epi32((__m128i)x, 0x39)
#define CHACHA_ROTV2(x) _mm_shuffle_epi32((__m128i)x, 0x4e)
#define CHACHA_ROTV3(x) _mm_shuffle_epi32((__m128i)x, 0x93)

    __m128i a = _mm_load_si128((__m128i*)(block));
    __m128i b = _mm_load_si128((__m128i*)(block + 4));
    __m128i c = _mm_load_si128((__m128i*)(block + 8));
    __m128i d = _mm_load_si128((__m128i*)(block + 12));

    for (int i = 0; i < R; i += 2) {
      a = _mm_add_epi32(a, b);
      d = _mm_xor_si128(d, a);
      d = _mm_roti_epi32(d, 16);
      c = _mm_add_epi32(c, d);
      b = _mm_xor_si128(b, c);
      b = _mm_roti_epi32(b, 12);
      a = _mm_add_epi32(a, b);
      d = _mm_xor_si128(d, a);
      d = _mm_roti_epi32(d, 8);
      c = _mm_add_epi32(c, d);
      b = _mm_xor_si128(b, c);
      b = _mm_roti_epi32(b, 7);

      b = CHACHA_ROTV1(b);
      c = CHACHA_ROTV2(c);
      d = CHACHA_ROTV3(d);

      a = _mm_add_epi32(a, b);
      d = _mm_xor_si128(d, a);
      d = _mm_roti_epi32(d, 16);
      c = _mm_add_epi32(c, d);
      b = _mm_xor_si128(b, c);
      b = _mm_roti_epi32(b, 12);
      a = _mm_add_epi32(a, b);
      d = _mm_xor_si128(d, a);
      d = _mm_roti_epi32(d, 8);
      c = _mm_add_epi32(c, d);
      b = _mm_xor_si128(b, c);
      b = _mm_roti_epi32(b, 7);

      b = CHACHA_ROTV3(b);
      c = CHACHA_ROTV2(c);
      d = CHACHA_ROTV1(d);
    }

    _mm_store_si128((__m128i*)(block), a);
    _mm_store_si128((__m128i*)(block + 4), b);
    _mm_store_si128((__m128i*)(block + 8), c);
    _mm_store_si128((__m128i*)(block + 12), d);

#undef CHACHA_ROTV3
#undef CHACHA_ROTV2
#undef CHACHA_ROTV1
  }

  alignas(16) uint32_t block[16];
  uint32_t keysetup[8];
  uint64_t ctr;
};

}  // namespace randen

#endif  // ENGINE_CHACHA_H_
