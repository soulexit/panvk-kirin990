/* lwgtest <icd.so> <lwg.spv>: 1024-invocation compute workgroups with subgroup
 * ops, shared memory and a barrier; checks every invocation's result bits. */
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CK(x) do { VkResult r_ = (x); if (r_) { printf("FAIL %s = %d\n", #x, r_); exit(1); } } while (0)
static PFN_vkGetInstanceProcAddr gipa; static VkInstance inst; static VkDevice dev;
#define I(n) PFN_##n n = (PFN_##n)gipa(inst, #n)
#define D(n) PFN_##n n = (PFN_##n)gdpa(dev, #n)
int main(int argc, char **argv)
{
   void *h = dlopen(argv[1], RTLD_NOW);
   gipa = (PFN_vkGetInstanceProcAddr)dlsym(h, "vk_icdGetInstanceProcAddr");
   I(vkCreateInstance);
   VkApplicationInfo ai = {VK_STRUCTURE_TYPE_APPLICATION_INFO, .apiVersion = VK_API_VERSION_1_1};
   VkInstanceCreateInfo ici = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &ai};
   CK(vkCreateInstance(&ici, NULL, &inst));
   I(vkEnumeratePhysicalDevices); I(vkCreateDevice); I(vkGetDeviceProcAddr); I(vkGetPhysicalDeviceMemoryProperties);
   PFN_vkGetDeviceProcAddr gdpa = vkGetDeviceProcAddr;
   uint32_t n = 1; VkPhysicalDevice pd; vkEnumeratePhysicalDevices(inst, &n, &pd);
   float prio = 1;
   VkDeviceQueueCreateInfo qci = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueCount = 1, .pQueuePriorities = &prio};
   VkDeviceCreateInfo dci = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .queueCreateInfoCount = 1, .pQueueCreateInfos = &qci};
   CK(vkCreateDevice(pd, &dci, NULL, &dev));
   D(vkCreateShaderModule); D(vkCreateDescriptorSetLayout); D(vkCreatePipelineLayout); D(vkCreateComputePipelines);
   D(vkCreateBuffer); D(vkGetBufferMemoryRequirements); D(vkAllocateMemory); D(vkBindBufferMemory); D(vkMapMemory);
   D(vkCreateDescriptorPool); D(vkAllocateDescriptorSets); D(vkUpdateDescriptorSets); D(vkCreateCommandPool);
   D(vkAllocateCommandBuffers); D(vkBeginCommandBuffer); D(vkEndCommandBuffer); D(vkCmdBindPipeline);
   D(vkCmdBindDescriptorSets); D(vkCmdDispatch); D(vkQueueSubmit); D(vkQueueWaitIdle); D(vkGetDeviceQueue);
   FILE *f = fopen(argv[2], "rb"); fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
   uint32_t *code = malloc(sz); if (fread(code, 1, sz, f) != (size_t)sz) return 1; fclose(f);
   VkShaderModuleCreateInfo smi = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = sz, .pCode = code};
   VkShaderModule sm; CK(vkCreateShaderModule(dev, &smi, NULL, &sm));
   VkDescriptorSetLayoutBinding lb = {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT};
   VkDescriptorSetLayoutCreateInfo dli = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = 1, .pBindings = &lb};
   VkDescriptorSetLayout dsl; CK(vkCreateDescriptorSetLayout(dev, &dli, NULL, &dsl));
   VkPipelineLayoutCreateInfo pli = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .setLayoutCount = 1, .pSetLayouts = &dsl};
   VkPipelineLayout pl; CK(vkCreatePipelineLayout(dev, &pli, NULL, &pl));
   VkComputePipelineCreateInfo cpi = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = sm, .pName = "main"}, .layout = pl};
   VkPipeline pipe; CK(vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &cpi, NULL, &pipe));
   const uint32_t groups = 5, total = groups * 1024;
   VkBufferCreateInfo bci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = total * 4, .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT};
   VkBuffer buf; CK(vkCreateBuffer(dev, &bci, NULL, &buf));
   VkMemoryRequirements mr; vkGetBufferMemoryRequirements(dev, buf, &mr);
   VkPhysicalDeviceMemoryProperties mp; vkGetPhysicalDeviceMemoryProperties(pd, &mp);
   uint32_t ti = 0; const VkMemoryPropertyFlags want = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
   while (!(mr.memoryTypeBits & (1u << ti)) || (mp.memoryTypes[ti].propertyFlags & want) != want) ti++;
   VkMemoryAllocateInfo mai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = mr.size, .memoryTypeIndex = ti};
   VkDeviceMemory mem; CK(vkAllocateMemory(dev, &mai, NULL, &mem)); CK(vkBindBufferMemory(dev, buf, mem, 0));
   uint32_t *map; CK(vkMapMemory(dev, mem, 0, VK_WHOLE_SIZE, 0, (void **)&map)); memset(map, 0xee, total * 4);
   VkDescriptorPoolSize ps = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1};
   VkDescriptorPoolCreateInfo dpi = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .maxSets = 1, .poolSizeCount = 1, .pPoolSizes = &ps};
   VkDescriptorPool dp; CK(vkCreateDescriptorPool(dev, &dpi, NULL, &dp));
   VkDescriptorSetAllocateInfo dsa = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, .descriptorPool = dp, .descriptorSetCount = 1, .pSetLayouts = &dsl};
   VkDescriptorSet set; CK(vkAllocateDescriptorSets(dev, &dsa, &set));
   VkDescriptorBufferInfo dbi = {buf, 0, VK_WHOLE_SIZE};
   VkWriteDescriptorSet wds = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = set, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &dbi};
   vkUpdateDescriptorSets(dev, 1, &wds, 0, NULL);
   VkQueue q; vkGetDeviceQueue(dev, 0, 0, &q);
   VkCommandPoolCreateInfo cpci = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
   VkCommandPool cp; CK(vkCreateCommandPool(dev, &cpci, NULL, &cp));
   VkCommandBufferAllocateInfo cbi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = cp, .commandBufferCount = 1};
   VkCommandBuffer cb; CK(vkAllocateCommandBuffers(dev, &cbi, &cb));
   VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; CK(vkBeginCommandBuffer(cb, &bi));
   vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipe);
   vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pl, 0, 1, &set, 0, NULL);
   vkCmdDispatch(cb, groups, 1, 1); CK(vkEndCommandBuffer(cb));
   VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cb};
   CK(vkQueueSubmit(q, 1, &si, VK_NULL_HANDLE)); CK(vkQueueWaitIdle(q));
   unsigned bad[5] = {0}, wrong = 0;
   for (uint32_t i = 0; i < total; i++) {
      for (int b = 0; b < 5; b++) if (!(map[i] & (1u << b))) bad[b]++;
      if (map[i] != 31) wrong++;
   }
   printf("%u invocations: %u wrong; subgroup_id %u, num_subgroups %u, subgroupAdd %u, ballot %u, shared+barrier %u\n",
          total, wrong, bad[0], bad[1], bad[2], bad[3], bad[4]);
   printf(wrong ? "FAIL\n" : "PASS\n");
   return wrong != 0;
}
