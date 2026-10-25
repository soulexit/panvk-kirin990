#include <android/hardware_buffer.h>
#include <cutils/native_handle.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
const native_handle_t *AHardwareBuffer_getNativeHandle(const AHardwareBuffer *);
int main() {
  uint64_t us[] = {0x300 /*color|sampled*/, 0x333, 0x900 | 0x300, 0x300 | (1ULL << 11) /*composer overlay*/, 0x30333};
  for (int i = 0; i < 5; i++) {
    AHardwareBuffer_Desc d = {964, 530, 1, 5, us[i]}; AHardwareBuffer *b;
    if (AHardwareBuffer_allocate(&d, &b)) { printf("usage %llx alloc fail\n", (unsigned long long)us[i]); continue; }
    const native_handle_t *h = AHardwareBuffer_getNativeHandle(b);
    printf("usage %llx numFds=%d numInts=%d\n", (unsigned long long)us[i], h->numFds, h->numInts);
    for (int k = 0; k < h->numFds; k++) {
      char p[64], l[256] = {0}; snprintf(p, 64, "/proc/self/fd/%d", h->data[k]); readlink(p, l, 255);
      struct stat st; fstat(h->data[k], &st);
      printf("  fd%d=%d size=%lld ino=%llu -> %s\n", k, h->data[k], (long long)lseek(h->data[k], 0, SEEK_END), (unsigned long long)st.st_ino, l);
    }
    const int *dd = h->data + h->numFds;
    printf("  ints[0..12]:"); for (int k = 0; k < 13; k++) printf(" %x", dd[k]); printf("\n");
    AHardwareBuffer_release(b);
  }
}
