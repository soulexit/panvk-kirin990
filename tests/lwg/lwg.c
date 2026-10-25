#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "r_spv.h"
static PFN_vkVoidFunction (*gipa)(VkInstance, const char *);
#define L(i, n) PFN_##n n = (PFN_##n)gipa(i, #n)
#define D(n) PFN_##n n = (PFN_##n)gd(dev, #n)
#define G 64
int main(int c, char **v) {
  setvbuf(stdout, NULL, _IONBF, 0);
  void *h = dlopen(v[1], RTLD_NOW); gipa = dlsym(h, "vk_icdGetInstanceProcAddr");
  L(NULL, vkCreateInstance);
  VkApplicationInfo ai = {VK_STRUCTURE_TYPE_APPLICATION_INFO, .apiVersion = VK_API_VERSION_1_1};
  VkInstanceCreateInfo ici = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &ai}; VkInstance in; vkCreateInstance(&ici, NULL, &in);
  L(in, vkEnumeratePhysicalDevices); L(in, vkCreateDevice); L(in, vkGetDeviceProcAddr);
  uint32_t n = 1; VkPhysicalDevice pd; vkEnumeratePhysicalDevices(in, &n, &pd);
  float pr = 1; VkDeviceQueueCreateInfo q = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueCount = 1, .pQueuePriorities = &pr};
  VkDeviceCreateInfo dci = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .queueCreateInfoCount = 1, .pQueueCreateInfos = &q};
  VkDevice dev; vkCreateDevice(pd, &dci, NULL, &dev); PFN_vkGetDeviceProcAddr gd = vkGetDeviceProcAddr;
  D(vkGetDeviceQueue); D(vkCreateBuffer); D(vkGetBufferMemoryRequirements); D(vkAllocateMemory); D(vkBindBufferMemory); D(vkMapMemory);
  D(vkCreateShaderModule); D(vkCreateDescriptorSetLayout); D(vkCreatePipelineLayout); D(vkCreateComputePipelines); D(vkCreateDescriptorPool);
  D(vkAllocateDescriptorSets); D(vkUpdateDescriptorSets); D(vkCreateCommandPool); D(vkAllocateCommandBuffers); D(vkBeginCommandBuffer);
  D(vkEndCommandBuffer); D(vkCmdBindPipeline); D(vkCmdBindDescriptorSets); D(vkCmdDispatch); D(vkQueueSubmit); D(vkQueueWaitIdle);
  VkQueue qu; vkGetDeviceQueue(dev, 0, 0, &qu);
  const size_t N = G * 960; VkBuffer b[2]; VkDeviceMemory m[2]; uint32_t *p[2];
  for (int i = 0; i < 2; i++) {
    VkBufferCreateInfo bc = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = N * 4, .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT};
    vkCreateBuffer(dev, &bc, NULL, &b[i]); VkMemoryRequirements mr; vkGetBufferMemoryRequirements(dev, b[i], &mr);
    VkMemoryAllocateInfo ma = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = mr.size, .memoryTypeIndex = 1};
    vkAllocateMemory(dev, &ma, NULL, &m[i]); vkBindBufferMemory(dev, b[i], m[i], 0); vkMapMemory(dev, m[i], 0, N * 4, 0, (void **)&p[i]);
  }
  srand(7); for (size_t i = 0; i < N; i++) { p[0][i] = rand() & 0xffff; p[1][i] = 0xdeadbeef; }
  VkShaderModuleCreateInfo smc = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = r_spv_len, .pCode = (const uint32_t *)r_spv}; VkShaderModule mod; vkCreateShaderModule(dev, &smc, NULL, &mod);
  VkDescriptorSetLayoutBinding bd[2] = {{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT}, {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT}};
  VkDescriptorSetLayoutCreateInfo lc = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = 2, .pBindings = bd}; VkDescriptorSetLayout dsl; vkCreateDescriptorSetLayout(dev, &lc, NULL, &dsl);
  VkPipelineLayoutCreateInfo plc = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .setLayoutCount = 1, .pSetLayouts = &dsl}; VkPipelineLayout pl; vkCreatePipelineLayout(dev, &plc, NULL, &pl);
  VkComputePipelineCreateInfo cpc = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, .stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = mod, .pName = "main"}, .layout = pl};
  VkPipeline pipe; printf("pipeline %d\n", vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &cpc, NULL, &pipe));
  if (c > 2) return 0;
  VkDescriptorPoolSize ps = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2}; VkDescriptorPoolCreateInfo dpc = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .maxSets = 1, .poolSizeCount = 1, .pPoolSizes = &ps}; VkDescriptorPool dp; vkCreateDescriptorPool(dev, &dpc, NULL, &dp);
  VkDescriptorSetAllocateInfo dsa = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, .descriptorPool = dp, .descriptorSetCount = 1, .pSetLayouts = &dsl}; VkDescriptorSet ds; vkAllocateDescriptorSets(dev, &dsa, &ds);
  VkDescriptorBufferInfo bi[2] = {{b[0], 0, VK_WHOLE_SIZE}, {b[1], 0, VK_WHOLE_SIZE}};
  VkWriteDescriptorSet w = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = ds, .dstBinding = 0, .descriptorCount = 2, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = bi}; vkUpdateDescriptorSets(dev, 1, &w, 0, NULL);
  VkCommandPoolCreateInfo cp = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO}; VkCommandPool pool; vkCreateCommandPool(dev, &cp, NULL, &pool);
  VkCommandBufferAllocateInfo ca = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = pool, .commandBufferCount = 1}; VkCommandBuffer cb; vkAllocateCommandBuffers(dev, &ca, &cb);
  VkCommandBufferBeginInfo cbi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; vkBeginCommandBuffer(cb, &cbi);
  vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipe); vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pl, 0, 1, &ds, 0, NULL);
  vkCmdDispatch(cb, G, 1, 1); vkEndCommandBuffer(cb);
  VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cb};
  printf("submit %d\n", vkQueueSubmit(qu, 1, &si, VK_NULL_HANDLE)); printf("wait %d\n", vkQueueWaitIdle(qu));
  int bad = 0;
  for (int g = 0; g < G; g++) {
    uint32_t *d = p[0] + g * 960, mn = ~0u, mx = 0, sm = 0;
    for (int i = 0; i < 960; i++) { mn = d[i] < mn ? d[i] : mn; mx = d[i] > mx ? d[i] : mx; sm += d[i]; }
    for (int l = 0; l < 960; l++) {
      uint32_t acc = 0; for (int i = 0; i < 8; i++) acc += d[(l + i * 37) % 960];
      uint32_t exp = d[l] * 3 + (l / 32) + d[959 - l] + acc + (l == 0 ? (mn ^ mx ^ sm) : 0);
      if (p[1][g * 960 + l] != exp) { if (bad < 5) printf("mismatch g=%d l=%d got %u want %u\n", g, l, p[1][g * 960 + l], exp); bad++; }
    }
  }
  printf("%s (%d bad of %zu)\n", bad ? "FAIL" : "PASS", bad, N);
  return bad != 0;
}
