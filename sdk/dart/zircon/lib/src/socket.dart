// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

part of zircon;

// ignore_for_file: constant_identifier_names
// ignore_for_file: public_member_api_docs

/// Typed wrapper around a Zircon socket object.
class Socket extends _HandleWrapper<Socket> {
  Socket(Handle? handle) : super(handle);

  // Signals
  static const int READABLE = ZX.SOCKET_READABLE;
  static const int WRITABLE = ZX.SOCKET_WRITABLE;
  static const int PEER_CLOSED = ZX.SOCKET_PEER_CLOSED;
  static const int WRITE_DISABLED = ZX.SOCKET_WRITE_DISABLED;
  static const int PEER_WRITE_DISABLED = ZX.SOCKET_PEER_WRITE_DISABLED;

  // Create options
  static const int STREAM = ZX.SOCKET_STREAM;
  static const int DATAGRAM = ZX.SOCKET_DATAGRAM;

  // Write options
  static const int DISPOSITION_WRITE_DISABLED =
      ZX.SOCKET_DISPOSITION_WRITE_DISABLED;
  static const int DISPOSITION_WRITE_ENABLED =
      ZX.SOCKET_DISPOSITION_WRITE_ENABLED;

  WriteResult write(ByteData data, [int options = 0]) {
    if (handle == null) {
      return const WriteResult(ZX.ERR_INVALID_ARGS);
    }

    return System.socketWrite(handle!, data, options);
  }

  ReadResult read(int numBytes) {
    if (handle == null) {
      return const ReadResult(ZX.ERR_INVALID_ARGS);
    }

    return System.socketRead(handle!, numBytes);
  }
}

/// Typed wrapper around a linked pair of socket objects and the
/// zx_socket_create() syscall used to create them.
class SocketPair extends _HandleWrapperPair<Socket?> {
  factory SocketPair([int options = Socket.STREAM]) {
    final HandlePairResult result = System.socketCreate(options);
    if (result.status == ZX.OK) {
      return SocketPair._(
          result.status, Socket(result.first), Socket(result.second));
    } else {
      return SocketPair._(result.status, null, null);
    }
  }

  SocketPair._(int status, Socket? first, Socket? second)
      : super._(status, first, second);
}
