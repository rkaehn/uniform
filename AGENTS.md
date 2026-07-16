
- Never write code comments.
- Vector math uses the `ext_vector_type` types from `include/cr_math.h` (`float2`, `int4`, etc). Use whole-vector operators (`a - b`, `a * s`) instead of component-wise expressions like `a.x - b.x, a.y - b.y`.
- `assert` is always compiled, including release builds. It is for library bugs and aborts. A condition a correct library only fails because of how it was called is a `cr_check(cond, fmt, ...)`. It reports the mistake, and the JS runtime turns it into an exception.
- Never look at other branches, commits, or worktrees if not explicitly requested.
- In `README.md`, only change code blocks, never the prose. The prose stays as it was in the commit that added the file.

- Always rebuild through the root `make` immediately before running a program. A binary in `<project>/build/products` may be stale.
- `make all` installs the runtimes into `bin/` and the libraries into `lib/`; `bd_target_add_uniform` bundles the runtimes from there, so run it after changing the runtime.
- `bd --run` on wasm serves the app at `https://localhost:9443/` with the certificate from `~/.config/uniform`, created once with `uniform --certificate`, and never opens a browser on the Mac; `--device` opens it in Safari on the connected device, `--device=<name>` picks one of several.
- Bound every run program with an explicit timeout sized to the expected runtime, patch in an exit condition (frame limit, auto-quit flag), or run in the background and kill. Ideally, this timeout should never be necessary and the program should exit on its own and end the task.
- Wait times for individual tasks (timeouts, sleeps, polling intervals, until-loops) are on the order of seconds, sized to how long the task actually takes. Do not guard steps with multi-minute waits; they only add idle time when something finishes early or hangs.
- `make test-*` compares the scenes against `TEST_REF` (default `ref@2x-20261025-062814`) in `~/Documents/refs`, which also exists in the tests app's Documents on the iPad; `--ref` in `ARGS` overrides it.
- The tests app with no mode flag starts the interactive scene browser and never exits on its own — automated runs must pass a mode (`--test`, `--capture`).
- On macOS, `--test` also runs the visual window tests in `tests/src/app`: they capture the app's own windows through ScreenCaptureKit (no screen recording permission needed) and drive resizes and input with posted `NSEvent`s, so they need the screen unlocked and at least 1000x700 points of free space at the top left of the main display.
- The web main thread never blocks on the GPU or the network: GPU readbacks and fetches complete through callbacks (`gpu_command_buffer_read_texture`, `cr_fetch`), and tests continue after GPU work through `ui_test_then_`.

- Perf numbers come from the connected iPad only.
- Rebooting the iPad resets its GPU performance state.

- Multiple sessions might be using the iPad or Mac, you must make sure that you are running the expected binary.
- Always install before launching.
