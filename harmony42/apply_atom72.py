#!/usr/bin/env python3
"""EXPERIMENTAL Huawei HarmonyOS 4.2 G76 JM full-atom-layout test.

On this device, the kbase 72-byte dependency-only probe returned the
matching atom number and udata with event DONE, but PanVK then sent real
jobs as 64-byte v2 atoms, all rejected with EINVAL.  MaliSX2's working
implementation selects the full 72-byte atom (64-byte stock atom plus
frame_nr tail) when the 72-byte probe succeeds.

PanVK already builds a 72-byte wrapper { u64 seq_nr; base_jd_atom atom }
when kd->atom_seq_nr is true. Only override stride selection on the
specific Mali-G76 ID recorded in the HarmonyOS logs and only when the
existing 72-byte probe selected the 64-byte v2/MediaTek path.

Validated on Huawei P40 Pro / HarmonyOS 4.2 with BannerHub 6 and ARMSX2; other devices and workloads require testing.
"""
from pathlib import Path
import sys

if len(sys.argv) != 2:
    raise SystemExit("Usage: apply_atom72.py PATH_TO_KBASE_KMOD_C")
p = Path(sys.argv[1])
s = p.read_text()
needle = "atom_stride = kbase_check_bifrost_atom_stride(fd, &atom_seq_nr);"
if s.count(needle) != 1:
    raise SystemExit(f"Unexpected PanVK JM stride-probe source ({s.count(needle)} matches); refusing patch.")
# Insert immediately after the existing optional stride-56/64 verification.
insert = r"""
   /*
    * HarmonyOS 4.2 / Kirin 990, Mali-G76 (gpu_id=0x72110000):
    * kbase accepted the 72-byte atom-layout probe with DONE + matching
    * udata, but EINVAL'd every real job when we selected 64-byte v2.
    *
    * MaliSX2's working code uses full 72-byte atoms (seq_nr+stock atom
    * + frame_nr tail) when the kernel accepts a 72-byte probe.
    * kd->atom_seq_nr makes kbase_kmod_job_submit[_dep]() wrap our
    * 64-byte base_jd_atom in a leading u64 seq_nr; sizeof=72.
    *
    * Override ONLY on G76 when the initial 72-byte probe chose 64.
    * PANVK_H42_ATOM72=0 restores the original selection for comparison.
    */
   if (!is_csf && atom_stride == 64 && raw_id &&
       (raw_id & 0xffffffffu) == 0x72110000u) {
      const char *h42_opt = getenv("PANVK_H42_ATOM72");
      if (!h42_opt || h42_opt[0] != '0') {
         atom_stride = 72;
         atom_seq_nr = true;
         mesa_logi("kbase: H42 Mali-G76 accepted 72-byte probe; "
                   "using full 72-byte job atoms (seq_nr+atom), "
                   "PANVK_H42_ATOM72=0 to disable");
      } else {
         mesa_logi("kbase: H42 full 72-byte atom test disabled "
                   "via PANVK_H42_ATOM72=0");
      }
   }
"""
s=s.replace(needle,needle+"\n"+insert,1)
p.write_text(s)
print("Applied guarded H42 72-byte atom-layout test to",p)
