// Copyright 2016 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#ifndef ZIRCON_KERNEL_LIB_CRYPTO_INCLUDE_LIB_CRYPTO_PRNG_H_
#define ZIRCON_KERNEL_LIB_CRYPTO_INCLUDE_LIB_CRYPTO_PRNG_H_

#include <lib/crypto/entropy_pool.h>
#include <lib/lazy_init/lazy_init.h>
#include <lib/zircon-internal/thread_annotations.h>
#include <stddef.h>
#include <stdint.h>

#include <kernel/event.h>
#include <kernel/mutex.h>
#include <kernel/spinlock.h>
#include <ktl/atomic.h>

namespace crypto {

typedef unsigned __int128 uint128_t;

// This exposes a (optionally-)thread-safe cryptographically secure PRNG.
// This PRNG must be seeded with at least 256 bits of "real" entropy before
// being used for cryptographic applications.
class Prng {
 public:
  // Tag object for constructing a non-thread-safe version.
  struct NonThreadSafeTag {};

  // Construct a thread-safe instance of the PRNG with the input pool. The pool provides must
  // provide at least 256 bits of entropy.
  explicit Prng(EntropyPool pool)
      : pool_(std::move(pool)),
        nonce_(0),
        is_thread_safe_(true),
        accumulated_(pool_.contents().size()) {}

  // Construct a thread-safe instance of the PRNG with the byte array at
  // |data| as the initial seed.  |size| is the length of |data| in bytes.
  Prng(const void* data, size_t size);

  // Construct a non-thread-safe instance of the PRNG with the byte array at
  // |data| as the initial seed.  |size| is the length of |data| in bytes.
  Prng(const void* data, size_t size, NonThreadSafeTag);

  ~Prng();

  // Re-seed the PRNG by mixing-in new entropy. |size| is in bytes.
  // |size| MUST NOT be greater than kMaxEntropy.
  // If size is 0, only hash of the current key is used to re-seed.
  void AddEntropy(const void* data, size_t size) TA_EXCL(add_entropy_lock_) TA_EXCL(pool_lock_);

  // Re-seed the PRNG by hash of the current key. This does not mix in new
  // entropy.
  void SelfReseed();

  // Get pseudo-random output of |size| bytes.  Blocks until at least
  // kMinEntropy bytes of entropy have been added to this PRNG.  |size| MUST
  // NOT be greater than kMaxDrawLen.  Identical PRNGs are only guaranteed to
  // produce identical output when given identical inputs.
  void Draw(void* out, size_t size) TA_EXCL(pool_lock_);

  // Return an integer in the range [0, exclusive_upper_bound) chosen
  // uniformly at random.  This is a wrapper for Draw(), and so has the same
  // caveats.
  uint64_t RandInt(uint64_t exclusive_upper_bound) TA_EXCL(pool_lock_);

  // Transitions the PRNG to thread-safe mode.  This asserts that the
  // instance is not yet thread-safe.
  void BecomeThreadSafe();

  // Inspect if this PRNG is thread-safe.
  bool is_thread_safe() const;

  // The minimum amount of entropy (in bytes) the generator requires before
  // Draw will return data.
  static constexpr uint64_t kMinEntropy = 32;

  // The maximum amount of entropy (in bytes) that can be submitted to
  // AddEntropy.  Anything above this will panic.
  static constexpr uint64_t kMaxEntropy = 1ULL << 30;

  // The maximum amount of pseudorandom data (in bytes) that can be drawn in
  // one call to Draw. This the limit imposed by the maximum number of bytes
  // that can be generated with a single key/nonce pair. Each request to Draw
  // uses a different key/nonce pair.  Anything above this will panic.
  static constexpr uint64_t kMaxDrawLen = 1ULL << 38;

 private:
  Prng(const Prng&) = delete;
  Prng& operator=(const Prng&) = delete;

  // Synchronizes calls to |AddEntropy|.
  DECLARE_MUTEX(Prng) add_entropy_lock_;

  // Controls access to |key_| |and nonce_|.
  DECLARE_SPINLOCK(Prng) pool_lock_;

  // ChaCha20 key(pool_) and nonce as described in RFC 7539.
  EntropyPool pool_ TA_GUARDED(pool_lock_);
  uint128_t nonce_ TA_GUARDED(pool_lock_);

  // Event used to signal when calls to |Draw| may proceed. This is initialized when
  // |BecomeThreadSafe| is called.
  lazy_init::LazyInit<Event> ready_;

  bool is_thread_safe_ = false;

  // Number of bytes of entropy added so far.
  ktl::atomic<size_t> accumulated_;
};

}  // namespace crypto

#endif  // ZIRCON_KERNEL_LIB_CRYPTO_INCLUDE_LIB_CRYPTO_PRNG_H_
