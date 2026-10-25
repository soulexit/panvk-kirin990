# NOTICE

- Repository scripts, tests and documentation: MIT (see `LICENSE`).
- `patches/` modify Mesa (https://gitlab.freedesktop.org/mesa/mesa); the patched code
  stays under Mesa's licenses (mostly MIT).
- The driver also contains the zenithblue-oss/panvk-kbase-android patch series
  (https://github.com/zenithblue-oss/panvk-kbase-android, commit 917b9b3b), applied by
  `scripts/prepare-tree.sh`; see that repository's NOTICE.md and LICENSES/.
- `tests/stress/dx7_harness.h`, `tests/ahb/ahb2.c`, `tests/ahb/ahb2d.c`, `tests/ahb/anb.c`
  are modified copies of zenithblue-oss/panvk-kbase-android test files (MIT); the
  PanProbe tests are built from that repository by `scripts/build-tests.sh`.
