# Copilot instructions for this workspace

Scope
- This workspace hosts multiple GNOME core projects checked out side‑by‑side: `glib/`, `gnome-shell/`, `mutter/`, plus `liburing/` used by tests/examples, BUT only focus on `glib`, ignore the other projects for now. Prefer minimal, reviewable edits aligned with GNOME conventions.

Build & test workflows (local)
- GLib uses Meson/Ninja.
  - Configure: `meson setup _build --buildtype debug --fatal-meson-warnings`
  - Build: `meson compile -C _build`
  - Tests: `meson test -C _build` (runs standard tests then flaky ones without failing the job)

Project conventions
- C (GLib/GObject/Mutter): follow GLib coding style; manage memory with `g_free`/`g_autoptr`; avoid ABI breaks.
- JS (gnome-shell/GJS): prefer `const/let`, imports via `imports.gi`, and existing utility modules; keep patches small.
- Windows support exists for GLib; see `docs/win32-build.md`.

Key files and dirs
- GLib: `meson.build`

Patterns to follow in PRs/MRs
- Keep changes focused; update or add minimal tests when touching public behavior.
- When CI needs artifacts from another pipeline, implement an API download step in `before_script` rather than `needs:` cross‑ref.
- Use exit code 77 to indicate intentional skip in CI helper scripts (e.g., duplicate comment already posted).

Notes for Windows/CI runners
- If only cmd.exe is available, use `.cmd` scripts and run them via `cmd.exe /c` or `call`. For PowerShell, use `[System.Environment]` APIs to read/write env vars.

If something is unclear (e.g., alternative local build flags, additional CI rules templates), tell us and we’ll refine these instructions.
