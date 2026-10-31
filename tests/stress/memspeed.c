/* memspeed icd: CPU copy bandwidth into/out of each HOST_VISIBLE memory type,
 * plus vkInvalidate/vkFlush cost for non-coherent types. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "dx7_harness.h"
#include <dlfcn.h>
static PFN_vkFlushMappedMemoryRanges vkFlushMappedMemoryRanges;
static PFN_vkInvalidateMappedMemoryRanges vkInvalidateMappedMemoryRanges;
static PFN_vkFreeMemory vkFreeMemory;

static double now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e3 + t.tv_nsec / 1e6; }

int main(int argc, char **argv) {
   if (argc < 2) { printf("usage: memspeed icd\n"); return 2; }
   struct dx7 t; VkPhysicalDeviceFeatures want = {0};
   dx7_init(&t, argv[1], &want);
   void *h = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
   PFN_vkGetInstanceProcAddr gipa = (PFN_vkGetInstanceProcAddr)dlsym(h, "vk_icdGetInstanceProcAddr");
   vkFlushMappedMemoryRanges = (void *)gipa(t.inst, "vkFlushMappedMemoryRanges");
   vkInvalidateMappedMemoryRanges = (void *)gipa(t.inst, "vkInvalidateMappedMemoryRanges");
   vkFreeMemory = (void *)gipa(t.inst, "vkFreeMemory");
   VkPhysicalDeviceMemoryProperties mp;
   vkGetPhysicalDeviceMemoryProperties(t.phys, &mp);
   const size_t sz = 8 << 20;
   uint8_t *host = aligned_alloc(64, sz); memset(host, 1, sz);
   for (uint32_t i = 0; i < mp.memoryTypeCount; i++) {
      VkMemoryPropertyFlags f = mp.memoryTypes[i].propertyFlags;
      if (!(f & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) continue;
      VkMemoryAllocateInfo ai = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = sz, .memoryTypeIndex = i};
      VkDeviceMemory m; if (vkAllocateMemory(t.dev, &ai, NULL, &m)) { printf("type %u alloc fail\n", i); continue; }
      void *p; vkMapMemory(t.dev, m, 0, VK_WHOLE_SIZE, 0, &p);
      VkMappedMemoryRange r = {.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, .memory = m, .offset = 0, .size = VK_WHOLE_SIZE};
      double w = 1e9, rd = 1e9, inv = 1e9, fl = 1e9, inv64 = 1e9;
      for (int k = 0; k < 5; k++) {
         double a = now_ms(); memcpy(p, host, sz); double b = now_ms(); if (b - a < w) w = b - a;
         a = now_ms(); vkFlushMappedMemoryRanges(t.dev, 1, &r); b = now_ms(); if (b - a < fl) fl = b - a;
         a = now_ms(); vkInvalidateMappedMemoryRanges(t.dev, 1, &r); b = now_ms(); if (b - a < inv) inv = b - a;
         a = now_ms(); memcpy(host, p, sz); b = now_ms(); if (b - a < rd) rd = b - a;
         VkMappedMemoryRange r2 = r; r2.size = 65536;
         a = now_ms(); vkInvalidateMappedMemoryRanges(t.dev, 1, &r2); b = now_ms(); if (b - a < inv64) inv64 = b - a;
      }
      printf("type %u flags 0x%x%s%s: write %.0f MB/s, read %.0f MB/s, flush 8MB %.3f ms, invalidate 8MB %.3f ms, invalidate 64KB %.3f ms\n",
             i, f, (f & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) ? " CACHED" : "", (f & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) ? " COHERENT" : "",
             sz / 1e3 / w, sz / 1e3 / rd, fl, inv, inv64);
      vkUnmapMemory(t.dev, m); vkFreeMemory(t.dev, m, NULL);
   }
   return 0;
}
