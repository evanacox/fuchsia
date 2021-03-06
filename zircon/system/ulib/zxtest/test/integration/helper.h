// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ZIRCON_SYSTEM_ULIB_ZXTEST_TEST_INTEGRATION_HELPER_H_
#define ZIRCON_SYSTEM_ULIB_ZXTEST_TEST_INTEGRATION_HELPER_H_

#include <stdlib.h>
#include <zircon/compiler.h>

#include <zxtest/zxtest.h>

#ifdef __cplusplus
namespace zxtest {
namespace test {

// Because we are checking that the user exposed macros work correctly, we need a way for checking
// that all went well. Independently of the body of the tests. This allows registering arbitrary
// function pointers which verify that the test described in each file suceeded.
void AddCheckFunction(void (*check)(void));

// Call all registered functions. Uses ZX_ASSERT for verification, so on fail this will crash. Its
// better than relying on the system under test to verify that the same system is working.
void CheckAll();

}  // namespace test
}  // namespace zxtest

#endif

__BEGIN_CDECLS
#define CHECKPOINT_REACHED true
#define CHECKPOINT_NOT_REACHED false
#define HAS_ERRORS 1u
#define NO_ERRORS 1u << 1
#define SKIPPED 1u << 2
#define NOT_SKIPPED 1u << 3

#define CHECK_ERROR() ZX_ASSERT_MSG(LIB_ZXTEST_TEST_HAS_ERRORS, "Expected errors, non registered.");
#define CHECK_NO_ERROR() ZX_ASSERT_MSG(!LIB_ZXTEST_TEST_HAS_ERRORS, "Unexpected errors.");
#define CHECK_SKIPPED() \
  ZX_ASSERT_MSG(LIB_ZXTEST_IS_SKIPPED, "Expected current test to be skipped.");
#define CHECK_NOT_SKIPPED() ZX_ASSERT_MSG(!LIB_ZXTEST_IS_SKIPPED, "Unexpected skip.");

typedef struct test_expectation {
  // Information of where the error happened.
  const char* filename;
  size_t line;
  const char* reason;

  // Flag marking whether the test reached a checkpoint.
  bool checkpoint_reached;
  // Whether the checkpoint should be reached.
  bool checkpoint_reached_expected;

  // Bitwise of what traits the test should have on exit. See: HAS_ERRORS, NO_ERRORS, etc.
  uint32_t expectation;
} test_expectation_t;

// Verifies that the expectations set for the |expectation| are met.
void verify_expectation(test_expectation_t* expectation);

// Macros to capture context, and validate.
#define TEST_EXPECTATION(checkpoint_reached_set, test_flags, err_desc)          \
  test_expectation_t _expectation __attribute__((cleanup(verify_expectation))); \
  _expectation.filename = __FILE__;                                             \
  _expectation.line = __LINE__;                                                 \
  _expectation.reason = err_desc;                                               \
  _expectation.expectation = test_flags;                                        \
  _expectation.checkpoint_reached_expected = checkpoint_reached_set;            \
  _expectation.checkpoint_reached = false

#define TEST_CHECKPOINT() _expectation.checkpoint_reached = true

void zxtest_add_check_function(void (*check)(void));
__END_CDECLS

#endif  // ZIRCON_SYSTEM_ULIB_ZXTEST_TEST_INTEGRATION_HELPER_H_
