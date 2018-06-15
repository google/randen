## Overview

What if we could default to attack-resistant random generators without excessive
CPU cost? We introduce 'Randen', a new generator with security guarantees; it
outperforms MT19937, pcg64_c32, Philox, ISAAC and ChaCha8 in real-world
benchmarks. This is made possible by AES hardware acceleration and a large
Feistel permutation.

## Related work

AES-CTR (encrypting a counter) is a well-known and easy to implement generator.
It has two known weaknesses:

-   A known-key distinguisher on 10-round, 128-bit AES [https://goo.gl/3xReB9].

-   No forward security/backtracking resistance: compromising the current state
    lets attackers distinguish prior outputs from random.

NIST 800-90a r1 [https://goo.gl/68Fwmv] is a standardized generator that ensures
backtracking resistance, but is not fast enough for a general-purpose generator
(5-10x slower than AES).

## Algorithm

The Randen generator is based upon three existing components:

1)  Reverie [https://eprint.iacr.org/2016/886.pdf] is a sponge-like generator
    that requires a cryptographic permutation. It improves upon "Provably Robust
    Sponge-Based PRNGs and KDFs" by achieving backtracking resistance with only
    a single permutation per buffer.

2)  Simpira v2 [https://eprint.iacr.org/2016/122.pdf] constructs up to 1024-bit
    permutations using an improved Generalized Feistel network with 2-round
    AES-128 functions. This Feistel block shuffle achieves diffusion sooner and
    is less vulnerable to sliced-biclique attacks than a Type-2 cyclic shuffle.

3)  "New criterion for diffusion property" [https://goo.gl/mLXH4f] shows that
    the same kind of improved Feistel block shuffle can be extended to 16
    branches, which enables a more efficient 2048-bit permutation.

We combine these by plugging the larger Simpira-like permutation into Reverie.

## Performance

The implementation targets x86 (Westmere), POWER 8 and ARM64.

x86 microbenchmark results (cpb=cycles per byte, MAD=median absolute deviation):

RNG | cpb | MAD
--- | --- | ---
Randen    |  1.54 | 0.002
pcg64_c32 |  0.78 | 0.003
mt19937_64|  1.79 | 0.001
ISAAC     |  4.08 | 0.006
Philox    |  4.70 | 0.003
ChaCha20  | 15.27 | 0.018
CTR-DRBG  | 16.80 | 0.009

x86 real-world benchmark (reservoir sampling):

RNG | cpb | MAD
--- | --- | ---
Randen    |  2.60 | 0.008
pcg64_c32 |  3.03 | 0.009
mt19937_64|  2.82 | 0.009
ISAAC     |  4.46 | 0.014
Philox    |  4.95 | 0.009
ChaCha20  | 13.46 | 0.017
CTR-DRBG  | 16.41 | 0.015

## Security

Randen is indistinguishable from random and backtracking-resistant. For more
details and benchmarks, please see
the paper "Randen - fast backtracking-resistant random generator with
AES+Feistel+Reverie" (pending publication).

## Usage

`make && bin/randen_benchmark`

Note that the code relies on compiler optimizations. Cycles per byte may
increase by factors of 1.6 when compiled with GCC 7.3, and 1.3 with
Clang 4.0.1. This can be mitigated by manually unrolling the loops.

## Third-party implementations / bindings

Thanks to Frank Denis for making us aware of these third-party implementations
or bindings. Note that the algorithm is still under review and subject to
change, but please feel free to get in touch or raise an issue and we'll
add yours as well.

By | Language | URL
--- | --- | ---
Frank Denis | C | https://github.com/jedisct1/randen-rng


This is not an official Google product.
