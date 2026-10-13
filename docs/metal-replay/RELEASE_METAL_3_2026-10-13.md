# RenderDoc Metal 1.46-metal.3 stage release

This narrow preview fixes rejection of legal Native compute threadgroup shapes. Converter default `tg_size` was treated as an exact API constraint. Replay now preserves the recorded API shape and checks Native axis/total limits, execution-width promises, dynamic memory and the existing resource/address/lifetime obligations.

The independent source shader and a second legal Native dispatch shape produce the same complete output. The previous metal.2 backend rejects this legal variant. This is the concrete release improvement; the larger UE Native/replay image mismatch is unresolved and is not counted as repaired.

The release starts from the metal.2 tag and selects only this production change, its Native capture variant and the corrected legacy test classification. CPU texture-identity observations, one-pixel Clear diagnostics, bitcode/AS experiments and material-list source audits are excluded and retained outside Git. The material-list audit ended with source locators only: no earliest API violation or viewport cause was established.

No capability declarations expand. Compute RT samples do not certify render-stage RT, Metal 3, AS internals, RT shader stepping or RT Pixel History. The app is Apple Silicon, ad-hoc signed and unnotarized. Full tagged-source/bundle tests and same-condition metal.2 performance comparison are required before publication. Source and product hashes will be carried in the attached release manifest and checksums.

The existing DX12/Vulkan separation of recorded dispatch commands and initial resource/address/lifetime restoration remains applicable. Metal validates the actual Native pipeline/device limits; converter dispatch defaults cannot supply an additional API restriction.
