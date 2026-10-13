# RenderDoc Metal 1.46-metal.3 stage release

This narrow preview fixes rejection of legal Native compute threadgroup shapes. Converter default `tg_size` was treated as an exact API constraint. Replay now preserves the recorded API shape and checks Native axis/total limits, execution-width promises, dynamic memory and the existing resource/address/lifetime obligations.

The independent source shader and a second legal Native dispatch shape produce the same complete output. The previous metal.2 backend rejects this legal variant. This is the concrete release improvement; the larger UE Native/replay image mismatch is unresolved and is not counted as repaired.

The release starts from the metal.2 tag and selects only this production change, its Native capture variant and the corrected legacy test classification. CPU texture-identity observations, one-pixel Clear diagnostics, bitcode/AS experiments and material-list source audits are excluded and retained outside Git. The material-list audit ended with source locators only: no earliest API violation or viewport cause was established.

No capability declarations expand. Compute RT samples do not certify render-stage RT, Metal 3, AS internals, RT shader stepping or RT Pixel History. The app is Apple Silicon, ad-hoc signed and unnotarized. Acceptance, same-condition metal.2 performance, source and final product hashes are recorded in the attached release manifest, checksums and GitHub release notes. A documentation/version-stamp rebuild is followed by affected acceptance against the final signed product; intermediate candidate results do not certify it.

The existing DX12/Vulkan separation of recorded dispatch commands and initial resource/address/lifetime restoration remains applicable. Metal validates the actual Native pipeline/device limits; converter dispatch defaults cannot supply an additional API restriction.

Related regression covers complete lawful indexed/nonindexed indirect output, current index bindings/actions, heap creation coherence, retirement, placement, mapped Shared publication, inline source recovery and samplers. Triangle/procedural compute RT output, TLAS bindings, events and EID0 are included. Real malformed ranges, missing sources, stale retirement generations and invalid sampler contracts remain pre-frame rejections. The legacy converted-root ABI-threadgroup case is reclassified as a legal control; its historical fixture GPU run is not claimed, while the independent source/binary Native contract is verified.

The matched small UE capture, fixed failed 1cb8/a4cb captures and public preview are checked separately. GPU execution, exact output and release acceptance remain distinct. Pixel-count variation between independent failed runs is not evidence of repair or regression; no tolerance is added. GUI capture/event/EID0/texture/current compute inspection, runtime relocation, Python and strict signatures are tested with the actual bundle. Physical first-present time remains unmeasured.

A first open is a fresh process on the same host with the same client and validation setting; three reopens close the controller and drain the autorelease pool. OS/Metal caches are not cleared. The 10-second first-open and 5-second reopen targets remain workload-specific; the larger anchor is above target. Tool starts, unique content, parsing, memory and the largest measured hotspot are retained in the manifest. This release makes no performance improvement claim over metal.2.

After publication, continue from the same fixed UE anchor and retained-error RDC. The next focus is the actual material/list GPU producer and physical version transitions. Source locators and correct local Clear/descriptor observations do not establish the viewport cause. No original-AS budget increase or new scene is part of this release.

## Published identity and final acceptance

Source/tag: `5178f08d04c5abdb9f2ce384d0fac55a65294474`. The post-publication documentation follow-up does not move this tag or change its assets.

- Contents/lib/librenderdoc.dylib: `e5cc44b8495204e61cef6f368a1a605cc6d32b00275371ed89120bcac2960ee6`
- Contents/MacOS/qrenderdoc: `978c4521d51f0f37507b5579ad7d444eb657a173d00ca5c9339a48be3ebb1b61`
- Contents/MacOS/renderdoccmd: `8aef8bc5429bf5e5e32c2c716a60ab1bdbfcd63b77e7d9f712e7b7450de6a83d`
- ZIP: `e5fcad1720480a8f0e1c01e75b2b2024c32c67e0d2fcdad44bcb0a9a7b43b168`
- Canonical bundle tree: `1ef065d9f992d37922fc461ef54cf97a0f1bbd43a7dc139e99f07814b94421dc`

Remote branch/tag and four asset API digests matched. All assets were downloaded again; every extracted file/link (1788 entries), checksum, strict signature and relocated UI/CLI version matched. Ninety final verification commands include25 real pre-frame restoration-negative commands. Matched small UE has six complete observations per version;1cb8 has six full FAIL per version, a4cb/public demo each three full FAIL on the final product. No new execution crash was observed.

| Capture / version | First | Reopen median | Root RSS |
| --- | ---: | ---: | ---: |
| small / metal2 | 6.972s | 4.136s | 2,177,613,824B |
| anchor / metal2 | 19.485s | 15.476s | 3,190,833,152B |
| small / metal3 | 6.107s | 3.891s | 2,202,910,720B |
| anchor / metal3 | 19.362s | 14.722s | 3,050,078,208B |

GUI startup-to-operable9.935s, rootRSS2105425920B, normal quit;384 original configuration files restored byte-exact. Physical first-present unmeasured. The largest first-open exclusive hotspot is dynamic binding/lifetime analysis; actual tool counts and full phase data remain in the attached manifest. Main-anchor improvement NOT_PROVEN, earliest restoration violationUNKNOWN, full adaptationINCOMPLETE.
