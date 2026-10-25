/* How long until polled completion (no blocking wait) sees finished GPU work? */
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <dlfcn.h>
#include <stdio.h>
#include <time.h>
static PFN_vkVoidFunction (*gipa)(VkInstance, const char *);
#define L(i, n) PFN_##n n = (PFN_##n)gipa(i, #n)
#define D(n) PFN_##n n = (PFN_##n)gd(dev, #n)
static double now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e3 + t.tv_nsec / 1e6; }
int main(int c, char **v) {
  setvbuf(stdout, NULL, _IONBF, 0);
  void *h = dlopen(v[1], RTLD_NOW); gipa = dlsym(h, "vk_icdGetInstanceProcAddr");
  L(NULL, vkCreateInstance);
  VkApplicationInfo ai = {VK_STRUCTURE_TYPE_APPLICATION_INFO, .apiVersion = VK_API_VERSION_1_1};
  VkInstanceCreateInfo ici = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &ai}; VkInstance in; vkCreateInstance(&ici, NULL, &in);
  L(in, vkEnumeratePhysicalDevices); L(in, vkCreateDevice); L(in, vkGetDeviceProcAddr);
  uint32_t n = 1; VkPhysicalDevice pd; vkEnumeratePhysicalDevices(in, &n, &pd);
  float pr = 1; VkDeviceQueueCreateInfo q = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueCount = 1, .pQueuePriorities = &pr};
  VkPhysicalDeviceVulkan12Features f12 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, .timelineSemaphore = 1, .hostQueryReset = 1};
  VkDeviceCreateInfo dci = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, &f12, .queueCreateInfoCount = 1, .pQueueCreateInfos = &q};
  VkDevice dev; vkCreateDevice(pd, &dci, NULL, &dev); PFN_vkGetDeviceProcAddr gd = vkGetDeviceProcAddr;
  D(vkGetDeviceQueue); D(vkCreateCommandPool); D(vkAllocateCommandBuffers); D(vkBeginCommandBuffer); D(vkEndCommandBuffer);
  D(vkCreateFence); D(vkGetFenceStatus); D(vkResetFences); D(vkQueueSubmit); D(vkWaitForFences); D(vkQueueWaitIdle);
  D(vkCreateBuffer); D(vkGetBufferMemoryRequirements); D(vkAllocateMemory); D(vkBindBufferMemory); D(vkCmdFillBuffer);
  D(vkCreateQueryPool); D(vkCmdResetQueryPool); D(vkCmdWriteTimestamp); D(vkGetQueryPoolResults);
  D(vkCreateEvent); D(vkCmdSetEvent); D(vkGetEventStatus); D(vkResetEvent);
  D(vkCreateSemaphore); D(vkGetSemaphoreCounterValue);
  VkQueue qu; vkGetDeviceQueue(dev, 0, 0, &qu);
  VkCommandPoolCreateInfo cp = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .flags = 2}; VkCommandPool pool; vkCreateCommandPool(dev, &cp, NULL, &pool);
  VkCommandBufferAllocateInfo ca = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = pool, .commandBufferCount = 1}; VkCommandBuffer cb; vkAllocateCommandBuffers(dev, &ca, &cb);
  VkBufferCreateInfo bc = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = 1 << 20, .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT}; VkBuffer b; vkCreateBuffer(dev, &bc, NULL, &b);
  VkMemoryRequirements mr; vkGetBufferMemoryRequirements(dev, b, &mr);
  VkMemoryAllocateInfo ma = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = mr.size, .memoryTypeIndex = 0}; VkDeviceMemory m; vkAllocateMemory(dev, &ma, NULL, &m); vkBindBufferMemory(dev, b, m, 0);
  VkQueryPoolCreateInfo qp = {VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, .queryType = VK_QUERY_TYPE_TIMESTAMP, .queryCount = 2}; VkQueryPool qpool; vkCreateQueryPool(dev, &qp, NULL, &qpool);
  VkEventCreateInfo ec = {VK_STRUCTURE_TYPE_EVENT_CREATE_INFO}; VkEvent ev; vkCreateEvent(dev, &ec, NULL, &ev);
  VkSemaphoreTypeCreateInfo stc = {VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO, .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE};
  VkSemaphoreCreateInfo sc = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, &stc}; VkSemaphore tl; vkCreateSemaphore(dev, &sc, NULL, &tl);
  VkFenceCreateInfo fc = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO}; VkFence f; vkCreateFence(dev, &fc, NULL, &f);
  VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  vkBeginCommandBuffer(cb, &bi); vkCmdResetQueryPool(cb, qpool, 0, 2); vkCmdFillBuffer(cb, b, 0, 1 << 20, 7);
  vkCmdWriteTimestamp(cb, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, qpool, 0); vkCmdSetEvent(cb, ev, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT); vkEndCommandBuffer(cb);
  for (int it = 0; it < 5; it++) {
    vkResetFences(dev, 1, &f); vkResetEvent(dev, ev);
    uint64_t val = it + 1;
    VkTimelineSemaphoreSubmitInfo ts = {VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO, .signalSemaphoreValueCount = 1, .pSignalSemaphoreValues = &val};
    VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO, &ts, .commandBufferCount = 1, .pCommandBuffers = &cb, .signalSemaphoreCount = 1, .pSignalSemaphores = &tl};
    printf("submit\n"); double t0 = now_ms(); printf("r=%d\n", vkQueueSubmit(qu, 1, &si, f));
    double tf = -1, tq = -1, te = -1, tt = -1; long spins = 0;
    while ((tf < 0 || tq < 0 || te < 0 || tt < 0) && now_ms() - t0 < 3000) {
      spins++; 
      if (tf < 0 && vkGetFenceStatus(dev, f) == VK_SUCCESS) tf = now_ms() - t0;
      uint64_t r[2]; if (tq < 0 && vkGetQueryPoolResults(dev, qpool, 0, 1, 16, r, 8, VK_QUERY_RESULT_64_BIT) == VK_SUCCESS) tq = now_ms() - t0;
      if (te < 0 && vkGetEventStatus(dev, ev) == VK_EVENT_SET) te = now_ms() - t0;
      tt = 0;
    }
    double tw0 = now_ms(); vkWaitForFences(dev, 1, &f, 1, ~0ull);
    printf("iter %d: polled fence %.2f ms, query %.2f ms, event %.2f ms, timeline %.2f ms (spins %ld)\n", it, tf, tq, te, tt, spins);
    vkQueueWaitIdle(qu);
  }
  return 0;
}
