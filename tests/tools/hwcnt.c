/* hwcnt [seconds] : sample the Mali-G76 hardware counters GPU-wide through the
 * kbase legacy counter reader while some other process uses the GPU, then print
 * every non-zero counter by block (front end, tiler, L2/MMU summed over slices,
 * shader core summed over cores). No root needed: any kbase client may set up a
 * reader. Counter names from ARM libGPUCounters (Mesa src/panfrost/perf). */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include "g76_names.h"

#define KB 0x80
struct ver { uint16_t major, minor; };
struct flags { uint32_t create_flags; };
struct reader_setup { uint32_t buffer_count, fe_bm, shader_bm, tiler_bm, mmu_l2_bm; };
#define KBASE_IOCTL_VERSION_CHECK _IOWR(KB, 0, struct ver)
#define KBASE_IOCTL_SET_FLAGS _IOW(KB, 1, struct flags)
#define KBASE_IOCTL_HWCNT_READER_SETUP _IOW(KB, 8, struct reader_setup)

#define RD 0xBE
struct meta { uint64_t timestamp; uint32_t event_id, buffer_idx; };
struct meta_cyc { uint64_t timestamp; uint32_t event_id, buffer_idx; uint64_t top, shader; };
#define READER_GET_BUFFER_SIZE _IOR(RD, 0x01, uint32_t)
#define READER_DUMP _IOW(RD, 0x10, uint32_t)
#define READER_CLEAR _IOW(RD, 0x11, uint32_t)
#define READER_GET_BUFFER _IOR(RD, 0x20, struct meta)
#define READER_PUT_BUFFER _IOW(RD, 0x21, struct meta)
#define READER_GET_BUFFER_CYC _IOR(RD, 0x20, struct meta_cyc)
#define READER_PUT_BUFFER_CYC _IOW(RD, 0x21, struct meta_cyc)

static const char *name_of(int block, int index)
{
   for (unsigned i = 0; i < sizeof(g76_names) / sizeof(g76_names[0]); i++)
      if (g76_names[i].block == block && g76_names[i].index == index)
         return g76_names[i].name;
   return NULL;
}

int main(int argc, char **argv)
{
   const double secs = argc > 1 ? atof(argv[1]) : 3;
   const int ncores = argc > 2 ? atoi(argv[2]) : 16;
   int fd = open("/dev/mali0", O_RDWR | O_CLOEXEC);
   if (fd < 0) { perror("open"); return 1; }
   struct ver v = {11, 0};
   if (ioctl(fd, KBASE_IOCTL_VERSION_CHECK, &v)) { perror("version"); return 1; }
   struct flags f = {0};
   if (ioctl(fd, KBASE_IOCTL_SET_FLAGS, &f)) { perror("set_flags"); return 1; }
   struct reader_setup rs = {8, ~0u, ~0u, ~0u, ~0u}; /* 8 x 5632 B = 11 pages */
   int rfd = ioctl(fd, KBASE_IOCTL_HWCNT_READER_SETUP, &rs);
   if (rfd < 0) { perror("reader_setup"); return 1; }
   uint32_t size = 0;
   if (ioctl(rfd, READER_GET_BUFFER_SIZE, &size)) { perror("bufsize"); return 1; }
   uint8_t *bufs = mmap(NULL, size * rs.buffer_count, PROT_READ, MAP_SHARED, rfd, 0);
   fprintf(stderr, "hwcnt: dump size %u x %u buffers\n", size, rs.buffer_count);
   if (bufs == MAP_FAILED) { perror("mmap"); return 1; }
   const int nblocks = size / 256, nl2 = nblocks - 2 - ncores;
   fprintf(stderr, "hwcnt: buffer %u bytes, %d blocks (%d L2, %d cores)\n", size, nblocks,
           nl2, ncores);

   uint32_t z = 0;
   ioctl(rfd, READER_CLEAR, &z);
   usleep((useconds_t)(secs * 1e6));
   if (ioctl(rfd, READER_DUMP, &z)) { perror("dump"); return 1; }

   struct meta_cyc mc = {0};
   int with_cycles = 1;
   if (ioctl(rfd, READER_GET_BUFFER_CYC, &mc)) {
      with_cycles = 0;
      struct meta m = {0};
      if (ioctl(rfd, READER_GET_BUFFER, &m)) { perror("get_buffer"); return 1; }
      mc.timestamp = m.timestamp; mc.buffer_idx = m.buffer_idx;
   }
   const uint32_t *d = (const uint32_t *)(bufs + (size_t)mc.buffer_idx * size);

   uint64_t sum[4][64] = {{0}};
   for (int b = 0; b < nblocks; b++) {
      const int kind = b == 0 ? 0 : b == 1 ? 1 : b < 2 + nl2 ? 2 : 3;
      for (int i = 4; i < 64; i++) sum[kind][i] += d[b * 64 + i];
   }
   static const char *kn[] = {"FE", "TILER", "L2", "SC"};
   printf("window %.2f s%s\n", secs, with_cycles ? "" : " (no cycle metadata)");
   for (int k = 0; k < 4; k++)
      for (int i = 4; i < 64; i++)
         if (sum[k][i]) {
            const char *n = name_of(k, i);
            printf("%-5s %-32s %14llu\n", kn[k], n ? n : "?", (unsigned long long)sum[k][i]);
         }
   if (with_cycles)
      printf("cycles top %llu shader %llu\n", (unsigned long long)mc.top,
             (unsigned long long)mc.shader);
   if (with_cycles) ioctl(rfd, READER_PUT_BUFFER_CYC, &mc);
   else { struct meta m = {mc.timestamp, 0, mc.buffer_idx}; ioctl(rfd, READER_PUT_BUFFER, &m); }
   return 0;
}
