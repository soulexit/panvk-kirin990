/* gsreplay <icd.so> <vert.spv> <geom.spv> <frag.spv>: draw GSREPLAY_TRIS triangles
 * (default 30000) through a captured geometry shader with a UBO at binding 3 and
 * SSBOs at bindings 4 and 5 (Eden's NieR:Automata GS), 16 viewports, into a
 * 256x256 target. Buffers are zero or GSREPLAY_FILL (hex word / "rand").
 * Reports whether the submit completes within 10 s. */
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CK(x) do { VkResult r_ = (x); if (r_) { printf("FAIL %s = %d\n", #x, r_); exit(1); } } while (0)
static PFN_vkGetInstanceProcAddr gipa;
static VkInstance inst;
static VkDevice dev;
#define I(n) PFN_##n n = (PFN_##n)gipa(inst, #n)
#define D(n) PFN_##n n = (PFN_##n)gdpa(dev, #n)

static uint32_t *load(const char *path, size_t *bytes)
{
   FILE *f = fopen(path, "rb");
   if (!f) { printf("cannot open %s\n", path); exit(1); }
   fseek(f, 0, SEEK_END); *bytes = ftell(f); fseek(f, 0, SEEK_SET);
   uint32_t *w = malloc(*bytes);
   if (fread(w, 1, *bytes, f) != *bytes) exit(1);
   fclose(f);
   return w;
}

static double now_ms(void)
{
   struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
   return t.tv_sec * 1e3 + t.tv_nsec / 1e6;
}

int main(int argc, char **argv)
{
   if (argc < 5) { printf("usage: gsreplay icd.so vert geom frag\n"); return 2; }
   void *h = dlopen(argv[1], RTLD_NOW);
   if (!h) { printf("dlopen %s\n", dlerror()); return 1; }
   gipa = (PFN_vkGetInstanceProcAddr)dlsym(h, "vk_icdGetInstanceProcAddr");
   I(vkCreateInstance);
   VkApplicationInfo ai = {VK_STRUCTURE_TYPE_APPLICATION_INFO, .apiVersion = VK_API_VERSION_1_3};
   VkInstanceCreateInfo ici = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &ai};
   CK(vkCreateInstance(&ici, NULL, &inst));
   I(vkEnumeratePhysicalDevices); I(vkCreateDevice); I(vkGetDeviceProcAddr);
   I(vkGetPhysicalDeviceFeatures2); I(vkGetPhysicalDeviceMemoryProperties);
   PFN_vkGetDeviceProcAddr gdpa = vkGetDeviceProcAddr;
   uint32_t n = 1; VkPhysicalDevice pd; vkEnumeratePhysicalDevices(inst, &n, &pd);

   VkPhysicalDeviceRobustness2FeaturesEXT r2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT};
   VkPhysicalDeviceVulkan12Features f12 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, &r2};
   VkPhysicalDeviceVulkan11Features f11 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES, &f12};
   VkPhysicalDeviceFeatures2 f2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &f11};
   vkGetPhysicalDeviceFeatures2(pd, &f2);
   const char *exts[] = {VK_EXT_ROBUSTNESS_2_EXTENSION_NAME};
   float prio = 1;
   VkDeviceQueueCreateInfo qci = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueCount = 1,
                                  .pQueuePriorities = &prio};
   VkDeviceCreateInfo dci = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, &f2, .queueCreateInfoCount = 1,
                             .pQueueCreateInfos = &qci, .enabledExtensionCount = 1,
                             .ppEnabledExtensionNames = exts};
   CK(vkCreateDevice(pd, &dci, NULL, &dev));
   printf("geometryShader %u multiViewport %u robustBufferAccess2 %u\n", f2.features.geometryShader,
          f2.features.multiViewport, r2.robustBufferAccess2);
   D(vkCreateShaderModule); D(vkCreateDescriptorSetLayout); D(vkCreatePipelineLayout);
   D(vkCreateGraphicsPipelines); D(vkCreateRenderPass); D(vkCreateBuffer);
   D(vkGetBufferMemoryRequirements); D(vkAllocateMemory); D(vkBindBufferMemory); D(vkMapMemory);
   D(vkCreateImage); D(vkGetImageMemoryRequirements); D(vkBindImageMemory); D(vkCreateImageView);
   D(vkCreateFramebuffer); D(vkCreateDescriptorPool); D(vkAllocateDescriptorSets);
   D(vkUpdateDescriptorSets); D(vkCreateCommandPool); D(vkAllocateCommandBuffers);
   D(vkBeginCommandBuffer); D(vkEndCommandBuffer); D(vkCmdBeginRenderPass); D(vkCmdEndRenderPass);
   D(vkCmdBindPipeline); D(vkCmdSetViewport); D(vkCmdSetScissor); D(vkCmdBindDescriptorSets);
   D(vkCmdDraw); D(vkCmdDrawIndexed); D(vkCmdBindIndexBuffer); D(vkQueueSubmit); D(vkGetDeviceQueue); D(vkCreateFence); D(vkWaitForFences);

   VkShaderModule sm[3];
   for (int i = 0; i < 3; i++) {
      size_t bytes; uint32_t *code = load(argv[2 + i], &bytes);
      VkShaderModuleCreateInfo smi = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = bytes, .pCode = code};
      CK(vkCreateShaderModule(dev, &smi, NULL, &sm[i]));
   }
   VkDescriptorSetLayoutBinding lb[3] = {
      {3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL_GRAPHICS},
      {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL_GRAPHICS},
      {5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL_GRAPHICS}};
   VkDescriptorSetLayoutCreateInfo dli = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                          .bindingCount = 3, .pBindings = lb};
   VkDescriptorSetLayout dsl; CK(vkCreateDescriptorSetLayout(dev, &dli, NULL, &dsl));
   VkPushConstantRange pcr = {VK_SHADER_STAGE_ALL_GRAPHICS, 0, 128};
   VkPipelineLayoutCreateInfo pli = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .setLayoutCount = 1,
                                     .pSetLayouts = &dsl, .pushConstantRangeCount = 1,
                                     .pPushConstantRanges = &pcr};
   VkPipelineLayout pl; CK(vkCreatePipelineLayout(dev, &pli, NULL, &pl));

   VkAttachmentDescription att = {0, VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_1_BIT,
      VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL};
   VkAttachmentReference ar = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
   VkSubpassDescription sp = {0, VK_PIPELINE_BIND_POINT_GRAPHICS, .colorAttachmentCount = 1,
                              .pColorAttachments = &ar};
   VkRenderPassCreateInfo rpi = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, .attachmentCount = 1,
                                 .pAttachments = &att, .subpassCount = 1, .pSubpasses = &sp};
   VkRenderPass rp; CK(vkCreateRenderPass(dev, &rpi, NULL, &rp));

   VkPipelineShaderStageCreateInfo st[3] = {
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = sm[0], .pName = "main"},
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_GEOMETRY_BIT, .module = sm[1], .pName = "main"},
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = sm[2], .pName = "main"}};
   VkPipelineVertexInputStateCreateInfo vi = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
   VkPipelineInputAssemblyStateCreateInfo ia = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                                                .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
   VkPipelineViewportStateCreateInfo vp = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                                           .viewportCount = 16, .scissorCount = 16};
   VkPipelineRasterizationStateCreateInfo rs = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                                                .polygonMode = VK_POLYGON_MODE_FILL, .lineWidth = 1};
   VkPipelineMultisampleStateCreateInfo ms = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                                              .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
   VkPipelineColorBlendAttachmentState cba = {.colorWriteMask = 0xf};
   VkPipelineColorBlendStateCreateInfo cb = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                                             .attachmentCount = 1, .pAttachments = &cba};
   VkDynamicState dyn[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
   VkPipelineDynamicStateCreateInfo dy = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                                          .dynamicStateCount = 2, .pDynamicStates = dyn};
   VkGraphicsPipelineCreateInfo gpi = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, .stageCount = 3,
      .pStages = st, .pVertexInputState = &vi, .pInputAssemblyState = &ia, .pViewportState = &vp,
      .pRasterizationState = &rs, .pMultisampleState = &ms, .pColorBlendState = &cb,
      .pDynamicState = &dy, .layout = pl, .renderPass = rp};
   VkPipeline pipe;
   double t0 = now_ms();
   CK(vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &gpi, NULL, &pipe));
   printf("pipeline %.1f ms\n", now_ms() - t0);

   VkPhysicalDeviceMemoryProperties mp; vkGetPhysicalDeviceMemoryProperties(pd, &mp);
#define ALLOC(req, mem, host) do { uint32_t ti_ = 0; \
      const VkMemoryPropertyFlags want_ = host ? (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) \
                                               : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT; \
      while (!((req).memoryTypeBits & (1u << ti_)) || (mp.memoryTypes[ti_].propertyFlags & want_) != want_) ti_++; \
      VkMemoryAllocateInfo mai_ = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = (req).size, .memoryTypeIndex = ti_}; \
      CK(vkAllocateMemory(dev, &mai_, NULL, &(mem))); } while (0)
   const VkDeviceSize bsize = 1 << 20;
   VkBufferCreateInfo bci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = bsize,
      .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
               VK_BUFFER_USAGE_INDEX_BUFFER_BIT};
   VkBuffer buf; CK(vkCreateBuffer(dev, &bci, NULL, &buf));
   VkMemoryRequirements mr; vkGetBufferMemoryRequirements(dev, buf, &mr);
   VkDeviceMemory bmem; ALLOC(mr, bmem, 1);
   CK(vkBindBufferMemory(dev, buf, bmem, 0));
   uint32_t *map; CK(vkMapMemory(dev, bmem, 0, VK_WHOLE_SIZE, 0, (void **)&map));
   memset(map, 0, bsize);
   if (getenv("GSREPLAY_FILL")) {
      const char *fv = getenv("GSREPLAY_FILL");
      uint32_t x = 12345, v = (uint32_t)strtoul(fv, NULL, 16);
      for (size_t i = 0; i < bsize / 4; i++) { x = x * 1103515245u + 12345u; map[i] = strcmp(fv, "rand") ? v : x; }
   }
   VkImageCreateInfo imci = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .imageType = VK_IMAGE_TYPE_2D,
      .format = VK_FORMAT_R8G8B8A8_UNORM, .extent = {256, 256, 1}, .mipLevels = 1, .arrayLayers = 1,
      .samples = VK_SAMPLE_COUNT_1_BIT, .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT};
   VkImage img; CK(vkCreateImage(dev, &imci, NULL, &img));
   VkMemoryRequirements imr; vkGetImageMemoryRequirements(dev, img, &imr);
   VkDeviceMemory imem; ALLOC(imr, imem, 0);
   CK(vkBindImageMemory(dev, img, imem, 0));
   VkImageViewCreateInfo ivi = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = img,
      .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = VK_FORMAT_R8G8B8A8_UNORM,
      .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
   VkImageView iv; CK(vkCreateImageView(dev, &ivi, NULL, &iv));
   VkFramebufferCreateInfo fbi = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, .renderPass = rp,
      .attachmentCount = 1, .pAttachments = &iv, .width = 256, .height = 256, .layers = 1};
   VkFramebuffer fb; CK(vkCreateFramebuffer(dev, &fbi, NULL, &fb));
   VkDescriptorPoolSize ps[2] = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2}};
   VkDescriptorPoolCreateInfo dpi = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .maxSets = 1,
                                     .poolSizeCount = 2, .pPoolSizes = ps};
   VkDescriptorPool dp; CK(vkCreateDescriptorPool(dev, &dpi, NULL, &dp));
   VkDescriptorSetAllocateInfo dsa = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, .descriptorPool = dp,
                                      .descriptorSetCount = 1, .pSetLayouts = &dsl};
   VkDescriptorSet set; CK(vkAllocateDescriptorSets(dev, &dsa, &set));
   VkDescriptorBufferInfo dbi[3] = {{buf, 0, 65536}, {buf, 65536, 65536 * 4}, {buf, 65536 * 8, 65536 * 4}};
   VkWriteDescriptorSet wds[3];
   for (int i = 0; i < 3; i++)
      wds[i] = (VkWriteDescriptorSet){VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = set,
         .dstBinding = 3 + i, .descriptorCount = 1,
         .descriptorType = i ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
         .pBufferInfo = &dbi[i]};
   vkUpdateDescriptorSets(dev, 3, wds, 0, NULL);

   VkQueue q; vkGetDeviceQueue(dev, 0, 0, &q);
   VkCommandPoolCreateInfo cpi = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
   VkCommandPool cp; CK(vkCreateCommandPool(dev, &cpi, NULL, &cp));
   VkCommandBufferAllocateInfo cbi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = cp,
                                      .commandBufferCount = 1};
   VkCommandBuffer cmd; CK(vkAllocateCommandBuffers(dev, &cbi, &cmd));
   VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
   CK(vkBeginCommandBuffer(cmd, &bi));
   VkClearValue clr = {0};
   VkRenderPassBeginInfo rbi = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, .renderPass = rp, .framebuffer = fb,
                                .renderArea = {{0, 0}, {256, 256}}, .clearValueCount = 1, .pClearValues = &clr};
   vkCmdBeginRenderPass(cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE);
   vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
   VkViewport vps[16]; VkRect2D scs[16];
   for (int i = 0; i < 16; i++) {
      vps[i] = (VkViewport){(i % 4) * 64.0f, (i / 4) * 64.0f, 64, 64, 0, 1};
      scs[i] = (VkRect2D){{0, 0}, {256, 256}};
   }
   vkCmdSetViewport(cmd, 0, 16, vps);
   vkCmdSetScissor(cmd, 0, 16, scs);
   vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pl, 0, 1, &set, 0, NULL);
   const uint32_t tris = getenv("GSREPLAY_TRIS") ? atoi(getenv("GSREPLAY_TRIS")) : 30000;
   const uint32_t draws = getenv("GSREPLAY_DRAWS") ? atoi(getenv("GSREPLAY_DRAWS")) : 1;
   const int indexed = getenv("GSREPLAY_INDEXED") != NULL;
   if (indexed) {
      /* 16-bit indices at 768 KiB: triangles over a range of GSREPLAY_RANGE
       * vertices (default 3000), shuffled. */
      const uint32_t range = getenv("GSREPLAY_RANGE") ? atoi(getenv("GSREPLAY_RANGE")) : 3000;
      uint16_t *ib = (uint16_t *)((char *)map + 768 * 1024);
      uint32_t x = 777;
      for (uint32_t i = 0; i < 3 * tris && i < 128 * 1024 / 2; i++) {
         x = x * 1103515245u + 12345u;
         ib[i] = (x >> 8) % range;
      }
      vkCmdBindIndexBuffer(cmd, buf, 768 * 1024, VK_INDEX_TYPE_UINT16);
   }
   for (uint32_t d = 0; d < draws; d++) {
      if (indexed)
         vkCmdDrawIndexed(cmd, 3 * tris, 1, 0, 0, 0);
      else
         vkCmdDraw(cmd, 3 * tris, 1, 0, 0);
   }
   vkCmdEndRenderPass(cmd);
   CK(vkEndCommandBuffer(cmd));
   VkFenceCreateInfo fci = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
   VkFence fence; CK(vkCreateFence(dev, &fci, NULL, &fence));
   VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cmd};
   t0 = now_ms();
   CK(vkQueueSubmit(q, 1, &si, fence));
   VkResult r = vkWaitForFences(dev, 1, &fence, VK_TRUE, 10000000000ull);
   printf("%u draws x %u triangles: %s after %.1f ms\n", draws, tris,
          r == VK_SUCCESS ? "done" : r == VK_TIMEOUT ? "TIMEOUT" : "ERROR", now_ms() - t0);
   printf(r ? "FAIL %d\n" : "PASS\n", r);
   return r ? 1 : 0;
}
