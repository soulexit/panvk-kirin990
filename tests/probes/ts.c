/* Timestamp query write, then a plain clear submit: does the context survive? */
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <dlfcn.h>
#include <stdio.h>
static PFN_vkVoidFunction (*gipa)(VkInstance, const char *);
#define L(i, n) PFN_##n n = (PFN_##n)gipa(i, #n)
#define D(n) PFN_##n n = (PFN_##n)gd(dev, #n)
int main(int c, char **v) {
  setvbuf(stdout, NULL, _IONBF, 0);
  void *h = dlopen(v[1], RTLD_NOW); gipa = dlsym(h, "vk_icdGetInstanceProcAddr");
  L(NULL, vkCreateInstance);
  VkApplicationInfo ai = {VK_STRUCTURE_TYPE_APPLICATION_INFO, .apiVersion = VK_API_VERSION_1_1};
  VkInstanceCreateInfo ici = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &ai}; VkInstance in; vkCreateInstance(&ici, NULL, &in);
  L(in, vkEnumeratePhysicalDevices); L(in, vkGetPhysicalDeviceProperties); L(in, vkGetPhysicalDeviceQueueFamilyProperties); L(in, vkCreateDevice); L(in, vkGetDeviceProcAddr);
  uint32_t n = 1; VkPhysicalDevice pd; vkEnumeratePhysicalDevices(in, &n, &pd);
  VkPhysicalDeviceProperties p; vkGetPhysicalDeviceProperties(pd, &p);
  VkQueueFamilyProperties qf; n = 1; vkGetPhysicalDeviceQueueFamilyProperties(pd, &n, &qf);
  printf("timestampPeriod=%f validBits=%u computeAndGraphics=%u\n", p.limits.timestampPeriod, qf.timestampValidBits, p.limits.timestampComputeAndGraphics);
  float pr = 1; VkDeviceQueueCreateInfo q = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueCount = 1, .pQueuePriorities = &pr};
  VkDeviceCreateInfo dci = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .queueCreateInfoCount = 1, .pQueueCreateInfos = &q};
  VkDevice dev; vkCreateDevice(pd, &dci, NULL, &dev); PFN_vkGetDeviceProcAddr gd = vkGetDeviceProcAddr;
  D(vkGetDeviceQueue); D(vkCreateCommandPool); D(vkAllocateCommandBuffers); D(vkBeginCommandBuffer); D(vkEndCommandBuffer);
  D(vkCreateQueryPool); D(vkCmdResetQueryPool); D(vkCmdWriteTimestamp); D(vkQueueSubmit); D(vkQueueWaitIdle); D(vkGetQueryPoolResults);
  D(vkCreateBuffer); D(vkGetBufferMemoryRequirements); D(vkAllocateMemory); D(vkBindBufferMemory); D(vkCmdFillBuffer); D(vkMapMemory);
  VkQueue qu; vkGetDeviceQueue(dev, 0, 0, &qu);
  VkCommandPoolCreateInfo cp = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO}; VkCommandPool pool; vkCreateCommandPool(dev, &cp, NULL, &pool);
  VkCommandBufferAllocateInfo ca = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = pool, .commandBufferCount = 2}; VkCommandBuffer cb[2]; vkAllocateCommandBuffers(dev, &ca, cb);
  VkQueryPoolCreateInfo qp = {VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, .queryType = VK_QUERY_TYPE_TIMESTAMP, .queryCount = 4}; VkQueryPool qpool; printf("querypool %d\n", vkCreateQueryPool(dev, &qp, NULL, &qpool));
  VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  vkBeginCommandBuffer(cb[0], &bi); vkCmdResetQueryPool(cb[0], qpool, 0, 4);
  vkCmdWriteTimestamp(cb[0], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, qpool, 0); vkCmdWriteTimestamp(cb[0], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, qpool, 1);
  printf("end %d\n", vkEndCommandBuffer(cb[0]));
  VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cb[0]};
  printf("submit ts %d\n", vkQueueSubmit(qu, 1, &si, VK_NULL_HANDLE)); printf("idle %d\n", vkQueueWaitIdle(qu));
  uint64_t r[2] = {0}; printf("results %d %llu %llu\n", vkGetQueryPoolResults(dev, qpool, 0, 2, 16, r, 8, VK_QUERY_RESULT_64_BIT), (unsigned long long)r[0], (unsigned long long)r[1]);
  /* follow-up job: fill a buffer */
  VkBufferCreateInfo bc = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = 4096, .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT}; VkBuffer b; vkCreateBuffer(dev, &bc, NULL, &b);
  VkMemoryRequirements mr; vkGetBufferMemoryRequirements(dev, b, &mr);
  VkMemoryAllocateInfo ma = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = mr.size, .memoryTypeIndex = 1}; VkDeviceMemory m; vkAllocateMemory(dev, &ma, NULL, &m); vkBindBufferMemory(dev, b, m, 0);
  vkBeginCommandBuffer(cb[1], &bi); vkCmdFillBuffer(cb[1], b, 0, 4096, 0x12345678); vkEndCommandBuffer(cb[1]);
  si.pCommandBuffers = &cb[1];
  for (int i = 0; i < 3; i++) { vkQueueSubmit(qu, 1, &si, VK_NULL_HANDLE); printf("fill idle %d\n", vkQueueWaitIdle(qu)); }
  uint32_t *pp; vkMapMemory(dev, m, 0, 4096, 0, (void **)&pp); printf("fill value %x %s\n", pp[100], pp[100] == 0x12345678 ? "PASS" : "FAIL");
  return 0;
}
