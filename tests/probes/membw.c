#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
static PFN_vkVoidFunction (*gipa)(VkInstance, const char *);
#define L(i, n) PFN_##n n = (PFN_##n)gipa(i, #n)
#define D(n) PFN_##n n = (PFN_##n)gd(dev, #n)
static double now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e3 + t.tv_nsec / 1e6; }
int main(int c, char **v) {
  void *h = dlopen(v[1], RTLD_NOW); gipa = dlsym(h, "vk_icdGetInstanceProcAddr");
  L(NULL, vkCreateInstance);
  VkApplicationInfo ai = {VK_STRUCTURE_TYPE_APPLICATION_INFO, .apiVersion = VK_API_VERSION_1_1};
  VkInstanceCreateInfo ici = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &ai}; VkInstance in; vkCreateInstance(&ici, NULL, &in);
  L(in, vkEnumeratePhysicalDevices); L(in, vkCreateDevice); L(in, vkGetDeviceProcAddr); L(in, vkGetPhysicalDeviceMemoryProperties);
  uint32_t n = 1; VkPhysicalDevice pd; vkEnumeratePhysicalDevices(in, &n, &pd);
  VkPhysicalDeviceMemoryProperties mp; vkGetPhysicalDeviceMemoryProperties(pd, &mp);
  float pr = 1; VkDeviceQueueCreateInfo q = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueCount = 1, .pQueuePriorities = &pr};
  VkDeviceCreateInfo dci = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .queueCreateInfoCount = 1, .pQueueCreateInfos = &q};
  VkDevice dev; vkCreateDevice(pd, &dci, NULL, &dev); PFN_vkGetDeviceProcAddr gd = vkGetDeviceProcAddr;
  D(vkAllocateMemory); D(vkMapMemory);
  const size_t SZ = 8 << 20; char *host = malloc(SZ); memset(host, 1, SZ);
  for (uint32_t t = 0; t < mp.memoryTypeCount; t++) {
    if (!(mp.memoryTypes[t].propertyFlags & 2)) continue;
    VkMemoryAllocateInfo ma = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = SZ, .memoryTypeIndex = t}; VkDeviceMemory m;
    if (vkAllocateMemory(dev, &ma, NULL, &m)) continue;
    volatile char *p; vkMapMemory(dev, m, 0, SZ, 0, (void **)&p);
    double t0 = now_ms(); for (int i = 0; i < 4; i++) memcpy((void *)p, host, SZ); double tw = now_ms() - t0;
    t0 = now_ms(); for (int i = 0; i < 4; i++) memcpy(host, (void *)p, SZ); double tr = now_ms() - t0;
    t0 = now_ms(); unsigned s = 0; for (size_t i = 0; i < SZ; i += 4) s += *(volatile unsigned *)(p + i); double ts = now_ms() - t0;
    printf("type %u flags %x: write %.0f MB/s, memcpy read %.0f MB/s, scalar read %.0f MB/s (%u)\n", t, mp.memoryTypes[t].propertyFlags,
           4 * SZ / 1e3 / tw, 4 * SZ / 1e3 / tr, SZ / 1e3 / ts, s & 1);
  }
  double t0 = now_ms(); char *h2 = malloc(SZ); for (int i = 0; i < 4; i++) memcpy(h2, host, SZ); printf("malloc memcpy %.0f MB/s\n", 4 * SZ / 1e3 / (now_ms() - t0));
  return 0;
}
