#include <android/hardware_buffer.h>
#include <cutils/native_handle.h>
#include <stdio.h>
const native_handle_t *AHardwareBuffer_getNativeHandle(const AHardwareBuffer *);
int main() {
  struct { const char *n; uint64_t u; } us[] = {
    {"gpu_rt|tex", 0x300}, {"gpu_rt|tex|composer", 0xb00}, {"gpu_rt|tex|composer|fb", 0xb00 | 0x200 | (1ULL << 9)},
    {"gpu_rt|tex|composer + SW_RW_OFTEN", 0xb00 | 0x33}, {"gpu_rt only", 0x200}, {"gpu_rt + SW_RW_OFTEN", 0x233}};
  for (int f = 1; f <= 5; f += 4)
  for (unsigned i = 0; i < sizeof(us) / sizeof(us[0]); i++) {
    AHardwareBuffer_Desc d = {1080, 2340, 1, f, us[i].u}; AHardwareBuffer *b;
    if (AHardwareBuffer_allocate(&d, &b)) { printf("fmt %d %-36s alloc fail\n", f, us[i].n); continue; }
    const int *dd = AHardwareBuffer_getNativeHandle(b)->data;
    printf("fmt %d %-36s isAfbc=%d pixelStride=%d\n", f, us[i].n, dd[6] & 0xff, dd[8]);
    AHardwareBuffer_release(b);
  }
}
