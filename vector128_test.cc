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

#include "vector128.h"

#include <stdio.h>
#include <stdlib.h>

namespace randen {
namespace {

#define ASSERT_TRUE(condition)                     \
  while (!(condition)) {                           \
    printf("Check failed at line %d\n", __LINE__); \
    abort();                                       \
  }

void TestLoadStore() {
  const int N = 4;
  alignas(16) uint64_t test_cases[N * 2] = {
      1, 2, 3, 4, 0x1234567890ABCDEFuLL, 0x2143658709BADCFEuLL};

  alignas(16) uint64_t stored[N * 2];
  for (int i = 0; i < N; ++i) {
    V v = Load(test_cases, i);
    Store(v, stored, i);

    ASSERT_TRUE(test_cases[2 * i + 0] == stored[2 * i + 0]);
    ASSERT_TRUE(test_cases[2 * i + 1] == stored[2 * i + 1]);
  }
}

void TestXor() {
  alignas(16) uint64_t test_cases[][3][2] = {
      {{1, 2}, {3, 4}, {2, 6}},
      {{0x1234567890ABCDEFuLL, 0x2143658709BADCFEuLL},
       {0x2143658709BADCFEuLL, 0x1234567890ABCDEFuLL},
       {0x337733ff99111111uLL, 0x337733ff99111111uLL}}};

  for (const auto& test_case : test_cases) {
    V v1 = Load(test_case[0], 0);
    V v2 = Load(test_case[1], 0);

    v1 ^= v2;
    alignas(16) uint64_t data_stored[2];
    Store(v1, data_stored, 0);

    ASSERT_TRUE(test_case[2][0] == data_stored[0]);
    ASSERT_TRUE(test_case[2][1] == data_stored[1]);
  }
}

void TestAes() {
  // This test also catches byte-order bugs in Load/Store functions
  alignas(16) uint64_t message[2] = {
      RANDEN_LE(0x8899AABBCCDDEEFFuLL, 0x0123456789ABCDEFuLL)};
  alignas(16) uint64_t key[2] = {
      RANDEN_LE(0x0022446688AACCEEuLL, 0x1133557799BBDDFFuLL)};
  alignas(16) uint64_t expected_result[2] = {
      RANDEN_LE(0x28E4EE1884504333uLL, 0x16AB0E57DFC442EDuLL)};

  V v_message = Load(message, 0);
  V v_key = Load(key, 0);
  V v_result = AES(v_message, v_key);

  alignas(16) uint64_t result[2];
  Store(v_result, result, 0);

  ASSERT_TRUE(expected_result[0] == result[0]);
  ASSERT_TRUE(expected_result[1] == result[1]);
}

void RunAll() {
  // Immediately output any results (for non-local runs).
  setvbuf(stdout, nullptr, _IONBF, 0);

  TestLoadStore();
  TestXor();
  TestAes();
}

}  // namespace
}  // namespace randen

int main(int argc, char* argv[]) {
  randen::RunAll();
  return 0;
}
