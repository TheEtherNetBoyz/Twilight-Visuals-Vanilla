# Third-party notices and provenance

## Licensing scope

The top-level CC0-1.0 dedication applies only to rights held by this project's contributors. It does not supersede third-party licenses or establish permission for game-derived material. Credit is not a substitute for permission.

## Dusk MFB / Dusklight-derived code

Source used during extraction: https://github.com/TheEtherNetBoyz/dusk-mfb and its upstream history (including https://github.com/matthewdavidrichardanderson/dusk-mfb).

The inspected Dusk MFB root `LICENSE.md` contains CC0-1.0. This standalone module was extracted from the host; see `PORT_STATUS.md`. `src/native_particles.cpp` includes adapted particle routines corresponding to `src/d/d_kankyo_rain.cpp` in the host, whose history traces to [zeldaret/tp](https://github.com/zeldaret/tp). That upstream project also publishes [CC0-1.0](https://github.com/zeldaret/tp/blob/main/LICENSE.md). Rendering helpers and engine-facing declarations were also extracted/adapted; do not represent the entire module as independently authored.

**Published-license audit (2026-09-03):** inspected MFB checkout `ff4c00c2b00c2aaf8175db79df9f35f39b9fb181`. Local history records the CC0 license addition at `64d92ceb9caf4f7f579da69045e83c7765184ff3` on July 25, 2023, and the current MFB license has the same Git blob hash as that version (`1625c1793607996fcfc46420e8aa2f3d2b7efd1e`). This identifies the audited checkout, not the exact extraction revision of every mod file. No conflicting file-specific license notice was identified in the inspected extracted source. The audit found no specific upstream-license conflict preventing source publication; it is not an exhaustive chain-of-title investigation or legal clearance. CC0 does not waive rights held by Nintendo or other third parties.

## dr_mp3 / minimp3

File: `src/third_party/dr_mp3.h`.

Upstream: https://github.com/mackron/dr_libs ; underlying decoder: https://github.com/lieff/minimp3 .

The bundled header provides a choice of public-domain dedication (Unlicense) or MIT No Attribution (MIT-0), with David Reid's notice. It also includes the minimp3 public-domain/CC0 notice. Those full notices remain embedded in the header and must not be replaced by a blanket project notice.

## Build dependencies

The separately installed Dusk SDK supplies engine headers/link stubs and build integration. The mod uses fmt and WebGPU headers through that SDK. Those dependencies are not relicensed by this project. Check the actual dependency versions and their license/distribution requirements before publishing compiled releases, including any statically linked code. No Aurora source is bundled in this source project.

The inspected fmt dependency's license credits Victor Zverovich and fmt contributors and uses MIT terms with an optional exception allowing compiled portions embedded in machine-executable output to be redistributed without the copyright and permission notices. Source redistribution remains subject to its source-license terms. See the license shipped with the dependency actually used for each release.

## Assets and music

The `res` directory currently contains only a placeholder. Music is loaded from user-supplied external files; no permission to redistribute those recordings or game assets is implied. Do not commit ROMs, ISOs, extracted archives, saves, commercial music, or migration backups.
