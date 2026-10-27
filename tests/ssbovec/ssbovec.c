/* ssbovec <icd.so> : robustBufferAccess2 check for panvk's merged 16-byte
 * SSBO loads. Each invocation i reads the 4 words at byte (i*3+1)*16 of a
 * storage buffer bound with a range that may end inside that vec4, and the
 * result must be the buffer word where the 4 bytes are in range, else 0. */
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ssbovec_spv.h"

static PFN_vkGetInstanceProcAddr gipa;
static VkInstance inst;
static VkDevice dev;
#define I(n) PFN_##n n = (PFN_##n)gipa(inst, #n)
#define D(n) PFN_##n n = (PFN_##n)gdpa(dev, #n)
#define CK(x) do { VkResult r_ = (x); if (r_) { printf("FAIL %s = %d\n", #x, r_); exit(1); } } while (0)
#define NINV 256u

int main(int argc, char **argv)
{
   void *h = dlopen(argv[1], RTLD_NOW);
   if (!h) { printf("dlopen %s\n", dlerror()); return 1; }
   gipa = (PFN_vkGetInstanceProcAddr)dlsym(h, "vk_icdGetInstanceProcAddr");
   I(vkCreateInstance);
   VkApplicationInfo ai = {VK_STRUCTURE_TYPE_APPLICATION_INFO, .apiVersion = VK_API_VERSION_1_1};
   VkInstanceCreateInfo ici = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &ai};
   CK(vkCreateInstance(&ici, NULL, &inst));
   I(vkEnumeratePhysicalDevices); I(vkCreateDevice); I(vkGetDeviceProcAddr);
   I(vkGetPhysicalDeviceMemoryProperties);
   PFN_vkGetDeviceProcAddr gdpa = vkGetDeviceProcAddr;
   uint32_t n = 1; VkPhysicalDevice pd; vkEnumeratePhysicalDevices(inst, &n, &pd);
   float prio = 1;
   VkDeviceQueueCreateInfo qci = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueCount = 1,
                                  .pQueuePriorities = &prio};
   VkPhysicalDeviceRobustness2FeaturesEXT r2 = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT, .robustBufferAccess2 = 1};
   VkPhysicalDeviceFeatures2 f2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &r2,
                                   .features.robustBufferAccess = 1};
   const char *ext = VK_EXT_ROBUSTNESS_2_EXTENSION_NAME;
   const int robust = !getenv("SSBOVEC_NOROBUST");
   VkDeviceCreateInfo dci = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, robust ? &f2 : NULL,
                             .queueCreateInfoCount = 1, .pQueueCreateInfos = &qci,
                             .enabledExtensionCount = robust, .ppEnabledExtensionNames = &ext};
   CK(vkCreateDevice(pd, &dci, NULL, &dev));
   D(vkGetDeviceQueue); D(vkCreateBuffer); D(vkGetBufferMemoryRequirements); D(vkAllocateMemory);
   D(vkBindBufferMemory); D(vkCreateShaderModule); D(vkCreateDescriptorSetLayout);
   D(vkCreatePipelineLayout); D(vkCreateComputePipelines); D(vkCreateDescriptorPool);
   D(vkAllocateDescriptorSets); D(vkUpdateDescriptorSets); D(vkCreateCommandPool);
   D(vkAllocateCommandBuffers); D(vkBeginCommandBuffer); D(vkEndCommandBuffer);
   D(vkCmdBindPipeline); D(vkCmdBindDescriptorSets); D(vkCmdDispatch); D(vkMapMemory);
   D(vkQueueSubmit); D(vkQueueWaitIdle); D(vkResetCommandBuffer);
   VkQueue q; vkGetDeviceQueue(dev, 0, 0, &q);
   VkPhysicalDeviceMemoryProperties mp; vkGetPhysicalDeviceMemoryProperties(pd, &mp);

   VkBuffer bufs[3]; void *maps[3];
   const VkDeviceSize sizes[3] = {NINV * 48 + 256, NINV * 16, NINV * 16};
   for (int k = 0; k < 3; k++) {
      VkBufferCreateInfo bci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = sizes[k],
                                .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT};
      CK(vkCreateBuffer(dev, &bci, NULL, &bufs[k]));
      VkMemoryRequirements mr; vkGetBufferMemoryRequirements(dev, bufs[k], &mr);
      const VkMemoryPropertyFlags want = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
      uint32_t ti = 0;
      while (!(mr.memoryTypeBits & (1u << ti)) || (mp.memoryTypes[ti].propertyFlags & want) != want) ti++;
      VkMemoryAllocateInfo mai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = mr.size,
                                  .memoryTypeIndex = ti};
      VkDeviceMemory mem; CK(vkAllocateMemory(dev, &mai, NULL, &mem));
      CK(vkBindBufferMemory(dev, bufs[k], mem, 0));
      CK(vkMapMemory(dev, mem, 0, VK_WHOLE_SIZE, 0, &maps[k]));
   }
   uint32_t *src = maps[0], *dst = maps[1], *dst2 = maps[2];
   for (uint32_t w = 0; w < sizes[0] / 4; w++) src[w] = 0x10000 + w;

   VkDescriptorSetLayoutBinding b[3] = {
      {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
      {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
      {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT}};
   VkDescriptorSetLayoutCreateInfo dli = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                          .bindingCount = 3, .pBindings = b};
   VkDescriptorSetLayout dsl; CK(vkCreateDescriptorSetLayout(dev, &dli, NULL, &dsl));
   VkPipelineLayoutCreateInfo pli = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                     .setLayoutCount = 1, .pSetLayouts = &dsl};
   VkPipelineLayout pl; CK(vkCreatePipelineLayout(dev, &pli, NULL, &pl));
   VkDescriptorPoolSize ps = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3};
   VkDescriptorPoolCreateInfo dpi = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .maxSets = 1,
                                     .poolSizeCount = 1, .pPoolSizes = &ps};
   VkDescriptorPool dp; CK(vkCreateDescriptorPool(dev, &dpi, NULL, &dp));
   VkDescriptorSetAllocateInfo dsa = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                      .descriptorPool = dp, .descriptorSetCount = 1,
                                      .pSetLayouts = &dsl};
   VkDescriptorSet ds; CK(vkAllocateDescriptorSets(dev, &dsa, &ds));

   VkShaderModuleCreateInfo smi = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                   .codeSize = ssbovec_spv_len, .pCode = (const uint32_t *)ssbovec_spv};
   VkShaderModule sm; CK(vkCreateShaderModule(dev, &smi, NULL, &sm));
   VkComputePipelineCreateInfo cpci = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = sm, .pName = "main"},
      .layout = pl};
   VkPipeline pipe; CK(vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &cpci, NULL, &pipe));
   VkCommandPoolCreateInfo cpi = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                  .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT};
   VkCommandPool cp; CK(vkCreateCommandPool(dev, &cpi, NULL, &cp));
   VkCommandBufferAllocateInfo cbi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                      .commandPool = cp, .commandBufferCount = 1};
   VkCommandBuffer cb; CK(vkAllocateCommandBuffers(dev, &cbi, &cb));

   /* descriptor offset, range: ranges ending at every 4 bytes of one vec4,
    * tiny ranges, and a non-zero 16-aligned descriptor offset. */
   const struct { VkDeviceSize off, range; } cases[] = {
      {0, 101 * 48 + 16}, {0, 101 * 48 + 20}, {0, 101 * 48 + 24}, {0, 101 * 48 + 28},
      {0, 101 * 48 + 32}, {0, 4}, {0, 16}, {0, 20}, {0, 28}, {0, 44},
      {16, 50 * 48 + 8}, {32, 50 * 48 + 12}, {48, 77 * 48 + 20}, {0, NINV * 48 + 64},
   };
   int bad_total = 0;
   for (unsigned c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
      VkDescriptorBufferInfo dbi[3] = {{bufs[0], cases[c].off, cases[c].range},
                                       {bufs[1], 0, VK_WHOLE_SIZE},
                                       {bufs[2], 0, VK_WHOLE_SIZE}};
      VkWriteDescriptorSet w = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = ds,
                                .descriptorCount = 3,
                                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                .pBufferInfo = dbi};
      vkUpdateDescriptorSets(dev, 1, &w, 0, NULL);
      memset(dst, 0xab, sizes[1]);
      memset(dst2, 0xcd, sizes[2]);
      vkResetCommandBuffer(cb, 0);
      VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
      vkBeginCommandBuffer(cb, &bi);
      vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipe);
      vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pl, 0, 1, &ds, 0, NULL);
      vkCmdDispatch(cb, NINV / 64, 1, 1);
      vkEndCommandBuffer(cb);
      VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1,
                         .pCommandBuffers = &cb};
      CK(vkQueueSubmit(q, 1, &si, VK_NULL_HANDLE));
      CK(vkQueueWaitIdle(q));
      int bad = 0;
      for (uint32_t i = 0; i < NINV; i++)
         for (uint32_t k = 0; k < 4; k++) {
            const uint64_t o = (uint64_t)(i * 3 + 1) * 16 + k * 4;
            const uint32_t want = o + 4 <= cases[c].range ? src[(cases[c].off + o) / 4] : 0;
            for (int v = 0; v < 2; v++) {
            const uint32_t got = (v ? dst2 : dst)[i * 4 + k];
            if (got != want && robust) {
               if (bad < 4)
                  printf("  v%d off %llu range %llu: inv %u word %u: got %08x want %08x\n", v,
                         (unsigned long long)cases[c].off, (unsigned long long)cases[c].range,
                         i, k, got, want);
               bad++;
            }
            }
         }
      printf("%s off %llu range %llu\n", bad ? "FAIL" : "ok  ", (unsigned long long)cases[c].off,
             (unsigned long long)cases[c].range);
      bad_total += bad;
   }
   printf(bad_total ? "FAIL %d words\n" : "PASS\n", bad_total);
   return bad_total != 0;
}
