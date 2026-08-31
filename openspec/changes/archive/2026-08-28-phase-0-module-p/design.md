## Context

See `proposal.md` for why this change exists. Specs lock the observable protocol; this document chooses where numbers live, which editor defaults are frozen, how far the existing checker is extended, and how P.3 may annotate stage-one order without renumbering tasks.

Current constraints that shape the approach:

- Editor already exposes `FrameStatistics`, Scene View overlay, and a 120/300 pipeline benchmark. No new HUD or timer is required for round 0.
- `Tools/verify_gpu_contracts.py` already hard-checks shared limits, binding/enum values, and paired struct **byte sizes**. It does not compare member sequences, and it does not assert `std140` / `std430` prefixes. Comments still mention retired `P.4` / `pre-refactor-validation-guards`.
- Scene View size is dock-dependent. `Runtime/Main.cpp` creates a 1900×1000 window; the rendered framebuffer is smaller and must be recorded, not assumed.
- Default editor settings already give all three paths the same post-process AA (`FXAA Quality`). Deferred rejects MSAA. That default is the comparable AA choice.
- `PHYSARA_STARTUP_SCENE` can load `Assets/Scenes/default.scene.json` without code changes.

## Goals / Non-Goals

**Goals:**

- Freeze one repeatable editor recipe for machine B, three paths, round 0.
- Put numbers and later overlays in `Docs/TaskDetail.md` under P.1 / P.2 / P.3, with a reserved empty machine A slot.
- Harden the existing checker just enough to match P.2: member-sequence (field) equality, SSBO/UBO layout prefixes, stale-id cleanup.
- After B round 0 exists, write a measured priority note for stage-one perf tasks. Keep Development task ids stable.

**Non-Goals:**

- New runtime counters, forced viewport resize API, golden images, or automated bench scripts.
- Committing `.rdc` binaries into the repo.
- Implementing stage-one or later perf work.
- Filling machine A in this change.
- Renumbering `Docs/Development.md` items.

## Decisions

### 1. Baseline ledger lives in TaskDetail, not a new data file

**Choice:** Extend `Docs/TaskDetail.md` P.1 with machine tables. P.2 records checker command, result, and leftover warnings. P.3 records the priority review. `Docs/Development.md` only flips `[ ]` → `[√]` and may grow a short “测量后优先级” pointer under module P.

**Why:** Development.md already says implementations and results belong in TaskDetail. One ledger matches the overlay rule: later rounds replace cells and bump `轮次`.

**Alternative considered:** `Docs/Baseline.md` or JSON under `openspec/`. Rejected — splits the official task record and invites stale copies.

### 2. Locked recipe is “editor defaults + scene + no navigation”

| Knob | Locked value |
|---|---|
| Build | Release, current MSVC / CMake recipe |
| Scene | `Assets/Scenes/default.scene.json` via `PHYSARA_STARTUP_SCENE` or Content Browser load |
| Window | `1900 × 1000`, do not resize after first dock layout |
| Display | Docked. After default layout appears, do not retile docks |
| Camera | Load scene, let editor frame to `MainCamera`, then do not orbit / fly / gizmo the camera. Pose is the scene file |
| Path variable | Only `Render Path`: Forward / Forward+ / Deferred |
| VSync | Off (editor default) |
| World Grid | Off while sampling (editor overlay, not scene content; benchmark already hides it) |
| Debug View | None |
| Skybox | On, default intensity / current default env path |
| Tonemap | ACES |
| Bloom | On, default threshold / knee / intensity / scatter |
| AA | FXAA Quality for all three paths (do not switch Forward to MSAA) |
| Shadows | On, PCF 3×3, 2048 / cascade, 4 cascades, other shadow sliders at defaults |
| Benchmark | Enabled, warmup 120, sample 300; wait until complete before copying median / p95 |

**Resolution lock:** copy the Scene View overlay `Size: W x H` from the first valid machine B sample. That pair becomes the locked framebuffer size. Later rounds on B must match it. If the dock is disturbed, restore layout or discard the sample.

**Why these defaults:** they are already the editor’s cold-start values except World Grid (turned off so overlay cost does not pollute path comparison) and the explicit scene load. Using FXAA Quality on every path avoids the Forward-only MSAA fork.

**Alternative considered:** Viewport Presentation mode for a stabler fullscreen size. Rejected for round 0 — daily cost includes docked UI, and Presentation would hide the panel the operator is copying from. Presentation remains allowed later as a *labeled extra* sample, never as a replacement for the locked docked record.

### 3. What each path row must contain

Copy from Scene View overlay + Renderer Settings benchmark + a short hardware block. Do not invent fields the HUD does not show.

**Environment (once per machine, reused by all paths):**

- Machine id (`B` or `A`), CPU, GPU, VRAM if known, RAM, OS, GPU driver, compiler/config (`Release`), git commit if available
- Window size, Scene View `W x H`, VSync, AA, shadow filter / resolution / cascades

**Per path:**

- CPU frame (`Frame` / scene / UI split), GPU frame
- CPU: collect, cluster, graph build/exec, plus path-relevant pass CPU
- GPU: shadow, skybox, opaque or GBuffer+deferred, transparent, world grid (expect ~0), post, bloom slices if shown
- Counts: draws/cmds, instances, tris, visible O/U/T, lights, clusters/refs/max/overflow, mat/sets, upload MB
- Benchmark CPU/GPU median and p95
- Operator visual note (pass / defect list)
- RenderDoc: `unavailable` until a capture is supplied; then filename + extracted pass times

Empty cells stay `unavailable`.

### 4. Round overlay is in-place cell replacement

Table header holds `轮次: N` plus date. Round 0 is the first complete B three-path set. After a later perf change, overwrite the same cells and increment `N`. No archive tables.

RenderDoc added to an otherwise unchanged round 0 does **not** bump `N` (spec: same conditions, late capture).

### 5. Checker: keep one script, add field sequence + layout prefix, delete stale ids

**Choice:** Continue using `Tools/verify_gpu_contracts.py` as the only gate. Extend it:

1. **Field check (hard):** for each paired struct, require equal member type sequences (and array dims), not only total size. Catches same-size field swaps the size check misses.
2. **Prefix check (hard):** known UBO `FrameUniforms` must be declared `std140`; known SSBOs (materials, cluster entries/indices, material texture indices, bindless handles, and any other script-listed storage buffers) must be declared `std430` with the matching binding macro. Missing or wrong qualifier fails.
3. **Stale ids:** rewrite comments in the script, `GPUContracts.hpp`, and `Docs/Physara.md` that still say `P.4` or `pre-refactor-validation-guards`. Living references to module P are allowed.
4. **Soft warnings stay soft:** overloaded bindings, unused binding macros, packing-risk member types.

Do **not** duplicate CPU `static_assert` offsetof tables into Python. Offsets stay a compile-time CPU guard; the script guards CPU↔GLSL agreement.

**Why not a second tool:** P.2 names this script. **Why not full semantic field names:** names differ (`MaterialGPUData` vs `MaterialData`); type sequence is the portable contract.

**Run:** `python Tools/verify_gpu_contracts.py` from repo root. Apply does not pass if the command exits non-zero.

### 6. P.3 annotates priority, does not renumber

After B round 0 numbers exist, write TaskDetail P.3 as an ordered attack list with one-line evidence (e.g. “shadow GPU dominates → keep 0.12 / 5.1 high”). Optionally add a bullet under Development module P pointing at that list.

Do not change task ids (`0.14`, `2.3`, …). Hygiene-only items (1.x, most H1/H2) stay in document order unless numbers prove they are on the hot path.

If rdc is still missing at review time, review proceeds from panel + benchmark and notes the missing capture.

### 7. Operator / apply split

| Work | Who |
|---|---|
| Release build, load scene, lock camera/docks, switch path, run benchmark, copy HUD, write visual note | Operator (human) |
| Supply three `.rdc` later | Operator |
| Script hardening, stale-id cleanup, TaskDetail/Development templates and sign-off text | Apply / implementation |
| Fill B numbers into the template | Operator, or apply once the operator pastes them |
| P.3 priority note | After B round 0 exists; draft from those numbers |

Machine A rows stay blank placeholders.

## Risks / Trade-offs

- **Docked Scene View size drifts** → Mitigation: first B sample defines `W x H`; later mismatch discards the sample. Do not retile.
- **Editor UI CPU sits in `Frame` time** → Mitigation: record UI build / scene / UI draw split; path comparison leans on renderer + GPU + benchmark (benchmark freezes view and hides overlays).
- **World Grid off ≠ daily default** → Mitigation: documented exception; cost is editor overlay. Re-enable only for a labeled extra sample.
- **No rdc at sign-off** → Mitigation: allowed; cells stay `unavailable`. Late capture updates the same round.
- **Checker still cannot see unnamed std140 padding bugs inside a matching type sequence** → Mitigation: CPU `static_assert` offsetof remains; script size + sequence catch the usual drift.
- **Low-power machine B numbers do not rank GPU-bound vs CPU-bound the same as machine A** → Mitigation: A slot reserved; P.3 must say “priority on B”. Do not pretend B is A.

## Migration Plan

1. Land checker + comment cleanup so P.2 can run immediately.
2. Insert empty P.1 tables (B filled later, A reserved) and P.2/P.3 stubs.
3. Operator captures B round 0 with the locked recipe; numbers overwrite placeholders.
4. Run checker; paste result into P.2.
5. Write P.3 from B numbers; mark Development P.1–P.3 done when those records exist.
6. Rollback: revert script/docs only. No runtime schema to migrate.

## Open Questions

- Machine A hardware string and capture date — fill when that machine is measured; does not affect this change’s tasks.
- Exact Scene View `W x H` on B — measured at first valid sample, then locked.
