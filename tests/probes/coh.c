/* Is cached host memory actually coherent with the GPU on this SoC? No flush/invalidate calls. */
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
static PFN_vkVoidFunction (*gipa)(VkInstance, const char *);
#define L(i, n) PFN_##n n = (PFN_##n)gipa(i, #n)
#define D(n) PFN_##n n = (PFN_##n)gd(dev, #n)
int main(int c, char **v) {
  void *h = dlopen(v[1], RTLD_NOW); gipa = dlsym(h, "vk_icdGetInstanceProcAddr");
  L(NULL, vkCreateInstance);
  VkApplicationInfo ai = {VK_STRUCTURE_TYPE_APPLICATION_INFO, .apiVersion = VK_API_VERSION_1_1};
  VkInstanceCreateInfo ici = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &ai}; VkInstance in; vkCreateInstance(&ici, NULL, &in);
  L(in, vkEnumeratePhysicalDevices); L(in, vkCreateDevice); L(in, vkGetDeviceProcAddr); L(in, vkGetPhysicalDeviceMemoryProperties);
  uint32_t n = 1; VkPhysicalDevice pd; vkEnumeratePhysicalDevices(in, &n, &pd);
  VkPhysicalDeviceMemoryProperties mp; vkGetPhysicalDeviceMemoryProperties(pd, &mp);
  uint32_t ct = ~0u; for (uint32_t t = 0; t < mp.memoryTypeCount; t++) if ((mp.memoryTypes[t].propertyFlags & 0xe) == 0xa) ct = t;
  if (ct == ~0u) { printf("no cached non-coherent type\n"); return 1; }
  float pr = 1; VkDeviceQueueCreateInfo q = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueCount = 1, .pQueuePriorities = &pr};
  VkDeviceCreateInfo dci = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .queueCreateInfoCount = 1, .pQueueCreateInfos = &q};
  VkDevice dev; vkCreateDevice(pd, &dci, NULL, &dev); PFN_vkGetDeviceProcAddr gd = vkGetDeviceProcAddr;
  D(vkGetDeviceQueue); D(vkCreateCommandPool); D(vkAllocateCommandBuffers); D(vkBeginCommandBuffer); D(vkEndCommandBuffer); D(vkResetCommandBuffer);
  D(vkQueueSubmit); D(vkQueueWaitIdle); D(vkCreateBuffer); D(vkAllocateMemory); D(vkBindBufferMemory); D(vkMapMemory); D(vkCmdCopyBuffer); D(vkCmdFillBuffer);
  VkQueue qu; vkGetDeviceQueue(dev, 0, 0, &qu);
  VkCommandPoolCreateInfo cp = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .flags = 2}; VkCommandPool pool; vkCreateCommandPool(dev, &cp, NULL, &pool);
  VkCommandBufferAllocateInfo ca = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = pool, .commandBufferCount = 1}; VkCommandBuffer cb; vkAllocateCommandBuffers(dev, &ca, &cb);
  const VkDeviceSize SZ = 1 << 16;
  VkBuffer a, b; VkDeviceMemory ma, mb; unsigned *pa, *pb;
  VkBufferCreateInfo bc = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = SZ, .usage = 3};
  VkMemoryAllocateInfo mi = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = SZ, .memoryTypeIndex = ct};
  vkCreateBuffer(dev, &bc, NULL, &a); vkAllocateMemory(dev, &mi, NULL, &ma); vkBindBufferMemory(dev, a, ma, 0); vkMapMemory(dev, ma, 0, SZ, 0, (void **)&pa);
  vkCreateBuffer(dev, &bc, NULL, &b); vkAllocateMemory(dev, &mi, NULL, &mb); vkBindBufferMemory(dev, b, mb, 0); vkMapMemory(dev, mb, 0, SZ, 0, (void **)&pb);
  VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cb};
  int bad_g = 0, bad_c = 0;
  for (int it = 1; it <= 50; it++) {
    /* CPU -> GPU: CPU writes a (cached, no flush); GPU copies a -> b */
    for (unsigned i = 0; i < SZ / 4; i++) pa[i] = it * 1000003u + i;
    (void)pb[0]; /* pull b into CPU cache: stale unless GPU writes invalidate it */
    vkResetCommandBuffer(cb, 0); vkBeginCommandBuffer(cb, &bi); VkBufferCopy r = {0, 0, SZ}; vkCmdCopyBuffer(cb, a, b, 1, &r); vkEndCommandBuffer(cb);
    vkQueueSubmit(qu, 1, &si, VK_NULL_HANDLE); vkQueueWaitIdle(qu);
    for (unsigned i = 0; i < SZ / 4; i++) if (pb[i] != it * 1000003u + i) { if (pb[i] == (it - 1) * 1000003u + i || pb[i] == 0) bad_c++; else bad_g++; break; }
  }
  printf("50 iters: mismatches stale/CPU-side=%d other=%d -> %s\n", bad_c, bad_g, (bad_c + bad_g) ? "NOT coherent" : "coherent in practice");
  return 0;
}
