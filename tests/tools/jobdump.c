/* jobdump: LD_PRELOAD into a Vulkan test to dump the Mali (Bifrost/JM) job chains
 * a driver submits through kbase, for comparing drivers on the same workload.
 * kbase maps job memory SAME_VA (GPU address == CPU address in this process), so
 * the chain can be followed from the CPU right at submit time.
 * JOBDUMP_SKIP=n: skip the first n submits (default 3), JOBDUMP_COUNT=n: dump n
 * submits (default 1), JOBDUMP_BYTES: bytes per job (default 384).
 * Output: stderr, one line per 16 bytes, prefixed by the job address and type. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>

/* struct kbase_ioctl_job_submit { __u64 addr; __u32 nr_atoms; __u32 stride; } */
#define KBASE_IOCTL_TYPE 0x80
#define KBASE_JOB_SUBMIT_NR 2

static const char *type_name(unsigned t)
{
   static const char *n[] = {"?", "null", "write", "cacheflush", "compute", "vertex",
                             "geometry", "tiler", "fused", "fragment", "idvs"};
   return t < sizeof(n) / sizeof(n[0]) ? n[t] : "?";
}

static void dump_chain(uint64_t jc, unsigned bytes)
{
   for (int j = 0; jc && j < 64; j++) {
      const uint8_t *p = (const uint8_t *)(uintptr_t)jc;
      const uint32_t w4 = *(const uint32_t *)(p + 0x10);
      const unsigned type = (w4 >> 1) & 0x7f;
      fprintf(stderr, "JOB %d @%llx type %u (%s)\n", j, (unsigned long long)jc, type,
              type_name(type));
      for (unsigned o = 0; o < bytes; o += 16) {
         const uint32_t *w = (const uint32_t *)(p + o);
         fprintf(stderr, "  +%03x: %08x %08x %08x %08x\n", o, w[0], w[1], w[2], w[3]);
      }
      if (type == 10) {
         /* IDVS: follow the vertex draw's State, Attribute buffers, Attributes,
          * Varying buffers, Varyings (offsets 0x138..0x158) and the fragment
          * draw's State (0xb8). */
         static const struct { unsigned off; const char *name; } ptrs[] = {
            {0x138, "vs_state"}, {0x140, "attr_bufs"}, {0x148, "attribs"},
            {0x150, "vary_bufs"}, {0x158, "varyings"}, {0xb8, "fs_state"},
            {0x48, "tiler_ctx"}};
         for (unsigned k = 0; k < 7; k++) {
            const uint64_t a = *(const uint64_t *)(p + ptrs[k].off);
            fprintf(stderr, "  %s @%llx\n", ptrs[k].name, (unsigned long long)a);
            if (k == 6 && a) {
               const uint64_t heap = *(const uint64_t *)(uintptr_t)(a + 0x18);
               fprintf(stderr, "  tiler_heap @%llx\n", (unsigned long long)heap);
               if (heap) {
                  const uint32_t *w = (const uint32_t *)(uintptr_t)heap;
                  fprintf(stderr, "    +00: %08x %08x %08x %08x %08x %08x %08x %08x\n",
                          w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7]);
               }
            }
            if (!a) continue;
            for (unsigned o = 0; o < 64; o += 16) {
               const uint32_t *w = (const uint32_t *)(uintptr_t)(a + o);
               fprintf(stderr, "    +%02x: %08x %08x %08x %08x\n", o, w[0], w[1], w[2], w[3]);
            }
         }
      }
      jc = *(const uint64_t *)(p + 0x18);
   }
}

int ioctl(int fd, int req_, ...)
{
   unsigned long req = (unsigned)req_;
   static int (*real)(int, int, ...);
   static int submits, skip = -1, count, bytes;
   if (!real) real = dlsym(RTLD_NEXT, "ioctl");
   va_list ap; va_start(ap, req_); void *arg = va_arg(ap, void *); va_end(ap);

   if (_IOC_TYPE(req) == KBASE_IOCTL_TYPE && _IOC_NR(req) == KBASE_JOB_SUBMIT_NR &&
       _IOC_SIZE(req) == 16) {
      if (skip < 0) {
         skip = getenv("JOBDUMP_SKIP") ? atoi(getenv("JOBDUMP_SKIP")) : 3;
         count = getenv("JOBDUMP_COUNT") ? atoi(getenv("JOBDUMP_COUNT")) : 1;
         bytes = getenv("JOBDUMP_BYTES") ? atoi(getenv("JOBDUMP_BYTES")) : 384;
      }
      const uint64_t *s = arg;
      const uint8_t *atoms = (const uint8_t *)(uintptr_t)s[0];
      const uint32_t nr = ((const uint32_t *)s)[2], stride = ((const uint32_t *)s)[3];
      /* JOBDUMP_LAG=1: dump the previous submit's chains now (after the GPU
       * ran and patched them), for drivers that fill descriptors on the GPU. */
      static uint64_t prev_jc[16]; static uint32_t prev_n;
      if (getenv("JOBDUMP_LAG")) {
         if (submits - 1 >= skip && submits - 1 < skip + count)
            for (uint32_t a = 0; a < prev_n; a++)
               if (prev_jc[a] && !(prev_jc[a] & 63)) {
                  fprintf(stderr, "LAGGED chain %u of submit %d\n", a, submits - 1);
                  dump_chain(prev_jc[a], bytes);
               }
         prev_n = nr < 16 ? nr : 16;
         for (uint32_t a = 0; a < prev_n; a++)
            prev_jc[a] = *(const uint64_t *)(atoms + (size_t)a * stride +
                                            (stride == 72 ? 8 : 0));
         submits++;
         return real(fd, req_, arg);
      }
      if (submits >= skip && submits < skip + count) {
         for (uint32_t a = 0; a < nr; a++) {
            const uint8_t *atom = atoms + (size_t)a * stride;
            /* base_jd_atom_v2 (stride 56): jc at 0, core_req at 48;
             * base_jd_atom (stride 72): seq_nr first, so jc at 8, core_req at 56. */
            const unsigned jo = stride == 72 ? 8 : 0;
            const uint64_t jc = *(const uint64_t *)(atom + jo);
            fprintf(stderr, "ATOM %u/%u submit %d stride %u jc %llx core_req %x\n", a, nr,
                    submits, stride, (unsigned long long)jc,
                    *(const uint32_t *)(atom + 48 + jo));
            if (jc & 63) { fprintf(stderr, "  (soft job, not dumped)\n"); continue; }
            dump_chain(jc, bytes);
         }
      }
      submits++;
   }
   return real(fd, req_, arg);
}
