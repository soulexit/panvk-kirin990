/* d3d12probe <icd.so>: check a Vulkan driver against VKD3D-Proton's hard device
 * requirements (libs/vkd3d/device.c), print the limits that decide its D3D12
 * feature level, and create the bindless descriptor layout VKD3D uses without
 * EXT_descriptor_heap: 1,000,000 mutable update-after-bind descriptors
 * (D3D12 resource binding tier 2 heap) and 2048 samplers. */
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

static PFN_vkGetInstanceProcAddr gipa;
static VkInstance inst;
#define I(n) PFN_##n n = (PFN_##n)gipa(inst, #n)
static int fails;
#define REQ(cond, what) do { int ok_ = !!(cond); printf("%s %s\n", ok_ ? "ok  " : "FAIL", what); fails += !ok_; } while (0)

int main(int argc, char **argv)
{
   void *h = dlopen(argv[1], RTLD_NOW);
   if (!h) { printf("dlopen %s\n", dlerror()); return 1; }
   gipa = (PFN_vkGetInstanceProcAddr)dlsym(h, "vk_icdGetInstanceProcAddr");
   I(vkCreateInstance);
   VkApplicationInfo ai = {VK_STRUCTURE_TYPE_APPLICATION_INFO, .apiVersion = VK_API_VERSION_1_3};
   VkInstanceCreateInfo ici = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &ai};
   if (vkCreateInstance(&ici, NULL, &inst)) { printf("no instance\n"); return 1; }
   I(vkEnumeratePhysicalDevices); I(vkGetPhysicalDeviceFeatures2); I(vkGetPhysicalDeviceProperties2);
   I(vkEnumerateDeviceExtensionProperties); I(vkCreateDevice); I(vkGetDeviceProcAddr);
   uint32_t n = 1; VkPhysicalDevice pd; vkEnumeratePhysicalDevices(inst, &n, &pd);

   VkPhysicalDeviceMutableDescriptorTypeFeaturesEXT mut = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MUTABLE_DESCRIPTOR_TYPE_FEATURES_EXT};
   VkPhysicalDeviceMaintenance6FeaturesKHR m6 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_6_FEATURES_KHR, &mut};
   VkPhysicalDeviceMaintenance5FeaturesKHR m5 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_FEATURES_KHR, &m6};
   VkPhysicalDeviceRobustness2FeaturesEXT r2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT, &m5};
   VkPhysicalDeviceVertexAttributeDivisorFeaturesKHR div = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES_KHR, &r2};
   VkPhysicalDeviceTransformFeedbackFeaturesEXT xfbf = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT, &div};
   VkPhysicalDeviceVulkan13Features f13 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES, &xfbf};
   VkPhysicalDeviceVulkan12Features f12 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, &f13};
   VkPhysicalDeviceVulkan11Features f11 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES, &f12};
   VkPhysicalDeviceFeatures2 f2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &f11};
   vkGetPhysicalDeviceFeatures2(pd, &f2);

   VkPhysicalDeviceTransformFeedbackPropertiesEXT xfbp = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_PROPERTIES_EXT};
   VkPhysicalDeviceVulkan13Properties p13 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES, &xfbp};
   VkPhysicalDeviceVulkan12Properties p12 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES, &p13};
   VkPhysicalDeviceVulkan11Properties p11 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES, &p12};
   VkPhysicalDeviceProperties2 p2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &p11};
   vkGetPhysicalDeviceProperties2(pd, &p2);
   const VkPhysicalDeviceLimits *L = &p2.properties.limits;

   uint32_t ne = 0; vkEnumerateDeviceExtensionProperties(pd, NULL, &ne, NULL);
   VkExtensionProperties ext[512]; if (ne > 512) ne = 512;
   vkEnumerateDeviceExtensionProperties(pd, NULL, &ne, ext);
#define HAS(e) ({ int f_ = 0; for (uint32_t i_ = 0; i_ < ne; i_++) f_ |= !strcmp(ext[i_].extensionName, e); f_; })

   printf("%s, Vulkan %u.%u.%u\n\n== VKD3D-Proton hard requirements\n", p2.properties.deviceName,
          VK_API_VERSION_MAJOR(p2.properties.apiVersion), VK_API_VERSION_MINOR(p2.properties.apiVersion),
          VK_API_VERSION_PATCH(p2.properties.apiVersion));
   REQ(p2.properties.apiVersion >= VK_API_VERSION_1_3, "Vulkan 1.3");
   REQ(div.vertexAttributeInstanceRateDivisor && div.vertexAttributeInstanceRateZeroDivisor, "vertex attribute divisor (incl. zero)");
   REQ(HAS("VK_EXT_transform_feedback") && xfbp.transformFeedbackQueries, "transform feedback queries");
   REQ((p13.storageTexelBufferOffsetSingleTexelAlignment || p13.storageTexelBufferOffsetAlignmentBytes == 1) &&
       (p13.uniformTexelBufferOffsetSingleTexelAlignment || p13.uniformTexelBufferOffsetAlignmentBytes == 1),
       "single texel buffer offset alignment");
   REQ(f12.samplerMirrorClampToEdge, "samplerMirrorClampToEdge");
   REQ(r2.robustBufferAccess2 && r2.robustImageAccess2, "robustBufferAccess2 + robustImageAccess2");
   REQ(r2.nullDescriptor, "nullDescriptor");
   REQ(f11.shaderDrawParameters, "shaderDrawParameters");
   REQ(HAS("VK_KHR_push_descriptor"), "VK_KHR_push_descriptor");
   REQ(m5.maintenance5 && m6.maintenance6, "maintenance5 + maintenance6");
   printf("\n== Needed in practice (bindless heaps)\n");
   REQ(f12.timelineSemaphore, "timelineSemaphore");
   REQ(f12.bufferDeviceAddress, "bufferDeviceAddress");
   REQ(f13.dynamicRendering && f13.synchronization2, "dynamicRendering + synchronization2");
   REQ(f12.descriptorIndexing && f12.runtimeDescriptorArray && f12.descriptorBindingPartiallyBound &&
       f12.descriptorBindingVariableDescriptorCount, "descriptor indexing (runtime arrays, partially bound, variable count)");
   REQ(f12.descriptorBindingSampledImageUpdateAfterBind && f12.descriptorBindingStorageImageUpdateAfterBind &&
       f12.descriptorBindingStorageBufferUpdateAfterBind && f12.descriptorBindingUniformTexelBufferUpdateAfterBind &&
       f12.descriptorBindingStorageTexelBufferUpdateAfterBind, "update-after-bind for images, buffers, texel buffers");
   REQ(f12.shaderSampledImageArrayNonUniformIndexing && f12.shaderStorageImageArrayNonUniformIndexing &&
       f12.shaderStorageBufferArrayNonUniformIndexing, "non-uniform descriptor indexing");
   REQ(mut.mutableDescriptorType, "mutableDescriptorType");
   printf("\n== Feature level inputs\n");
   printf("logicOp %u vertexPipelineStoresAndAtomics %u fragmentStoresAndAtomics %u geometryShader %u "
          "tessellationShader %u shaderInt64 %u shaderInt16 %u shaderFloat16 %u shaderInt8 %u "
          "shaderStorageImageWriteWithoutFormat %u shaderStorageImageReadWithoutFormat %u depthBounds %u "
          "independentBlend %u dualSrcBlend %u multiViewport %u sparseBinding %u\n",
          f2.features.logicOp, f2.features.vertexPipelineStoresAndAtomics, f2.features.fragmentStoresAndAtomics,
          f2.features.geometryShader, f2.features.tessellationShader, f2.features.shaderInt64,
          f2.features.shaderInt16, f12.shaderFloat16, f12.shaderInt8,
          f2.features.shaderStorageImageWriteWithoutFormat, f2.features.shaderStorageImageReadWithoutFormat,
          f2.features.depthBounds, f2.features.independentBlend, f2.features.dualSrcBlend,
          f2.features.multiViewport, f2.features.sparseBinding);
   printf("maxPerStage SB %u SI %u (FL 11_1 needs 64 each)\n", L->maxPerStageDescriptorStorageBuffers,
          L->maxPerStageDescriptorStorageImages);
   printf("UAB maxPerStage: samplers %u sampled %u storageimg %u storagebuf %u ubo %u resources %u\n",
          p12.maxPerStageDescriptorUpdateAfterBindSamplers, p12.maxPerStageDescriptorUpdateAfterBindSampledImages,
          p12.maxPerStageDescriptorUpdateAfterBindStorageImages, p12.maxPerStageDescriptorUpdateAfterBindStorageBuffers,
          p12.maxPerStageDescriptorUpdateAfterBindUniformBuffers, p12.maxPerStageUpdateAfterBindResources);
   printf("UAB maxDescriptorSet: samplers %u sampled %u storageimg %u storagebuf %u; maxUpdateAfterBindDescriptorsInAllPools %u\n",
          p12.maxDescriptorSetUpdateAfterBindSamplers, p12.maxDescriptorSetUpdateAfterBindSampledImages,
          p12.maxDescriptorSetUpdateAfterBindStorageImages, p12.maxDescriptorSetUpdateAfterBindStorageBuffers,
          p12.maxUpdateAfterBindDescriptorsInAllPools);
   printf("subgroup size %u, maxBoundDescriptorSets %u, maxPushConstantsSize %u, maxComputeSharedMemorySize %u\n",
          p11.subgroupSize, L->maxBoundDescriptorSets, L->maxPushConstantsSize, L->maxComputeSharedMemorySize);

   /* Device with what VKD3D enables for bindless, then the heap layouts. */
   float prio = 1;
   VkDeviceQueueCreateInfo qci = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueCount = 1, .pQueuePriorities = &prio};
   const char *exts[] = {"VK_EXT_mutable_descriptor_type", "VK_KHR_push_descriptor", "VK_EXT_robustness2"};
   VkPhysicalDeviceMutableDescriptorTypeFeaturesEXT mut_on = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MUTABLE_DESCRIPTOR_TYPE_FEATURES_EXT, .mutableDescriptorType = 1};
   VkPhysicalDeviceVulkan12Features f12_on = f12; f12_on.pNext = &mut_on;
   VkPhysicalDeviceFeatures2 f2_on = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &f12_on, f2.features};
   VkDeviceCreateInfo dci = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, &f2_on, .queueCreateInfoCount = 1,
                             .pQueueCreateInfos = &qci, .enabledExtensionCount = 3, .ppEnabledExtensionNames = exts};
   VkDevice dev;
   VkResult r = vkCreateDevice(pd, &dci, NULL, &dev);
   printf("\n== Bindless heap layouts\nvkCreateDevice %d\n", r);
   if (r) return 1;
   PFN_vkCreateDescriptorSetLayout cdsl = (PFN_vkCreateDescriptorSetLayout)vkGetDeviceProcAddr(dev, "vkCreateDescriptorSetLayout");
   PFN_vkCreateDescriptorPool cdp = (PFN_vkCreateDescriptorPool)vkGetDeviceProcAddr(dev, "vkCreateDescriptorPool");
   PFN_vkAllocateDescriptorSets ads = (PFN_vkAllocateDescriptorSets)vkGetDeviceProcAddr(dev, "vkAllocateDescriptorSets");

   static const VkDescriptorType mtypes[] = {
      VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
      VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER};
   VkMutableDescriptorTypeListEXT ml = {6, mtypes};
   VkMutableDescriptorTypeCreateInfoEXT mci = {VK_STRUCTURE_TYPE_MUTABLE_DESCRIPTOR_TYPE_CREATE_INFO_EXT, NULL, 1, &ml};
   const VkDescriptorBindingFlags bflags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
      VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
   struct { const char *name; VkDescriptorType type; uint32_t count; int mutable_; } heaps[] = {
      {"CBV_SRV_UAV mutable x1000000", VK_DESCRIPTOR_TYPE_MUTABLE_EXT, 1000000, 1},
      {"CBV_SRV_UAV sampled x1000000", VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000000, 0},
      {"CBV_SRV_UAV storage img x1000000", VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000000, 0},
      {"SAMPLER x2048", VK_DESCRIPTOR_TYPE_SAMPLER, 2048, 0},
   };
   for (unsigned i = 0; i < sizeof(heaps) / sizeof(heaps[0]); i++) {
      VkDescriptorSetLayoutBindingFlagsCreateInfo bfi = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                                                          heaps[i].mutable_ ? &mci : NULL, 1, &bflags};
      VkDescriptorSetLayoutBinding b = {0, heaps[i].type, heaps[i].count, VK_SHADER_STAGE_ALL, NULL};
      VkDescriptorSetLayoutCreateInfo lci = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, &bfi,
                                             VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT, 1, &b};
      VkDescriptorSetLayout l;
      VkResult lr = cdsl(dev, &lci, NULL, &l);
      VkResult pr = -1, sr = -1;
      if (!lr) {
         VkDescriptorPoolSize ps = {heaps[i].type, heaps[i].count};
         VkDescriptorPoolCreateInfo pci = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                           heaps[i].mutable_ ? &mci : NULL,
                                           VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT, 1, 1, &ps};
         VkDescriptorPool p;
         pr = cdp(dev, &pci, NULL, &p);
         if (!pr) {
            VkDescriptorSetVariableDescriptorCountAllocateInfo vc = {
               VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO, NULL, 1, &heaps[i].count};
            VkDescriptorSetAllocateInfo sai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, &vc, p, 1, &l};
            VkDescriptorSet s;
            sr = ads(dev, &sai, &s);
         }
      }
      printf("%s %s: layout %d pool %d set %d\n", (!lr && !pr && !sr) ? "ok  " : "FAIL", heaps[i].name, lr, pr, sr);
      fails += !!(lr || pr || sr);
   }
   printf("\n%d failed\n", fails);
   return 0;
}
