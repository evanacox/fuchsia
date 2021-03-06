// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/async-loop/cpp/loop.h>
#include <lib/fdio/directory.h>
#include <lib/fs-pty/service.h>
#include <lib/zx/eventpair.h>
#include <zircon/process.h>
#include <zircon/processargs.h>
#include <zircon/status.h>
#include <zircon/syscalls.h>

#include "pty-server-vnode.h"
#include "pty-server.h"
#include "src/lib/storage/vfs/cpp/pseudo_dir.h"
#include "src/lib/storage/vfs/cpp/synchronous_vfs.h"
#include "src/lib/storage/vfs/cpp/vfs_types.h"
#include "src/lib/storage/vfs/cpp/vnode.h"
#include "src/sys/lib/stdout-to-debuglog/cpp/stdout-to-debuglog.h"

// Each Open() on this Vnode redirects to a new PtyServerVnode
class PtyGeneratingVnode : public fs::Vnode {
 public:
  PtyGeneratingVnode(fs::FuchsiaVfs* vfs) : vfs_(vfs) {}
  ~PtyGeneratingVnode() override = default;

  zx_status_t GetNodeInfoForProtocol([[maybe_unused]] fs::VnodeProtocol protocol,
                                     [[maybe_unused]] fs::Rights rights,
                                     fs::VnodeRepresentation* info) override {
    // This should only actually be seen by something querying with VNODE_REF_ONLY.
    *info = fs::VnodeRepresentation::Tty{.event = {}};
    return ZX_OK;
  }

  fs::VnodeProtocolSet GetProtocols() const final { return fs::VnodeProtocol::kTty; }

 private:
  zx_status_t OpenNode(ValidatedOptions options, fbl::RefPtr<Vnode>* out_redirect) override {
    fbl::RefPtr<PtyServer> server;
    zx_status_t status = PtyServer::Create(&server, vfs_);
    if (status != ZX_OK) {
      return status;
    }
    *out_redirect = fbl::MakeRefCounted<PtyServerVnode>(std::move(server));
    return ZX_OK;
  }

  fs::FuchsiaVfs* vfs_;
};

int main(int argc, const char** argv) {
  zx_status_t status;
  if ((status = StdoutToDebuglog::Init()) != ZX_OK) {
    printf("console: failed to init stdout to debuglog: %s\n", zx_status_get_string(status));
    return -1;
  }

  async::Loop loop(&kAsyncLoopConfigNeverAttachToThread);
  async_dispatcher_t* dispatcher = loop.dispatcher();
  fs::SynchronousVfs vfs(dispatcher);

  auto root_dir = fbl::MakeRefCounted<fs::PseudoDir>();
  auto svc_dir = fbl::MakeRefCounted<fs::PseudoDir>();
  if ((status = root_dir->AddEntry("svc", svc_dir)) != ZX_OK) {
    printf("console: failed to add svc to root dir: %s\n", zx_status_get_string(status));
    return -1;
  }
  if ((status = svc_dir->AddEntry(fidl::DiscoverableProtocolName<fuchsia_hardware_pty::Device>,
                                  fbl::MakeRefCounted<PtyGeneratingVnode>(&vfs))) != ZX_OK) {
    printf("console: failed to add %s to svc dir: %s\n",
           fidl::DiscoverableProtocolName<fuchsia_hardware_pty::Device>,
           zx_status_get_string(status));
    return -1;
  }

  if ((status = vfs.ServeDirectory(root_dir, fidl::ServerEnd<fuchsia_io::Directory>(zx::channel(
                                                 zx_take_startup_handle(PA_DIRECTORY_REQUEST))))) !=
      ZX_OK) {
    printf("console: failed to serve startup handle: %s\n", zx_status_get_string(status));
    return -1;
  }

  ZX_ASSERT_MSG((status = loop.Run()) == ZX_OK, "%s", zx_status_get_string(status));
  return 0;
}
