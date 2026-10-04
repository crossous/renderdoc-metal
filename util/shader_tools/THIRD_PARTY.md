# Bundled Metal shader tools

RenderDoc runs these executables as separate processes. Their licenses are independent
of RenderDoc's MIT license. No private shaders or captures are included in their source archives.

- metal2vulkan, steelbrain/metal2vulkan commit `43c46ac8a24adf1a6e872b8a52c706ec9614fad0`,
  LGPL-3.0-or-later. Its unmodified corresponding source archive and the dependency lockfile
  accompany the executable in `source/`. Rebuild with Rust >= 1.87:
  extract `metal2vulkan.tar.gz`, copy `source/Cargo.lock` to its root, then run
  `cargo build --locked --release --features serde -p metal2vulkan -j 2`.
  Users may replace `bin/metal2vulkan` with their own compatible build; the processor invokes
  it by relative path. Tool invocation needs no Rust runtime.
  Source: https://github.com/steelbrain/metal2vulkan/tree/43c46ac8a24adf1a6e872b8a52c706ec9614fad0
- SPIRV-Cross, KhronosGroup, `vulkan-sdk-1.4.357.0`, Apache-2.0 and upstream notices.
  Source: https://github.com/KhronosGroup/SPIRV-Cross/tree/vulkan-sdk-1.4.357.0
- SPIRV-Tools and SPIRV-Headers, KhronosGroup, `vulkan-sdk-1.4.357.0`, Apache-2.0
  and upstream notices. Source archives accompany the executables.
  Source: https://github.com/KhronosGroup/SPIRV-Tools/tree/vulkan-sdk-1.4.357.0
- Apple Metal compiler and metal-objdump are supplied by Xcode, not redistributed.

`manifest.json` records the architecture, revisions and hashes. Build dependencies and
full source are described in the fork's `util/shader_tools/README.md`.
