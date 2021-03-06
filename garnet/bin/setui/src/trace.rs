// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This mod provides utilities for simplifying the collection of trace events.

/// A tracing nonce (not more than once) is a unique token used to keep track of async traces since
/// normal tracing gets confused by the interleaved events that occur in async contexts. Use this
/// nonce for any async tracing events you want to be correlated together. When generated by
/// [fuchsia_trace::generate_nonce], it is guaranteed to be unique for all other nonces returned by
/// that function.
pub type TracingNonce = u64;

/// This macro simplifies collecting async trace events. It uses "setui" as the category name.
#[macro_export]
macro_rules! trace {
    ($nonce:expr, $name:expr $(, $key:expr => $val:expr)* $(,)?) => {
        let _guard = ::fuchsia_trace::async_enter!($nonce, "setui", $name $(, $key => $val)*);
    }
}

/// This macro simplifies collecting async trace events. It returns a guard that can be used to
/// control the scope of the tracing event. It uses "setui" as the category name.
#[macro_export]
macro_rules! trace_guard {
    ($nonce:expr, $name:expr $(, $key:expr => $val:expr)* $(,)?) => {
        ::fuchsia_trace::async_enter!($nonce, "setui", $name $(, $key => $val)*)
    }
}
