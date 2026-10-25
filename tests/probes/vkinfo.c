/* Minimal ICD probe: dlopen a Vulkan ICD directly and dump caps. */
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef PFN_vkVoidFunction (*PFN_icdGIPA)(VkInstance, const char*);
static PFN_icdGIPA gipa;
#define L(inst,n) PFN_##n n = (PFN_##n)gipa(inst, #n)
int main(int argc, char **argv) {
  const char *so = argc > 1 ? argv[1] : "./libvulkan_panfrost.so";
  void *h = dlopen(so, RTLD_NOW | RTLD_LOCAL);
  if (!h) { printf("dlopen: %s\n", dlerror()); return 1; }
  gipa = (PFN_icdGIPA)dlsym(h, "vk_icdGetInstanceProcAddr");
  if (!gipa) { printf("no vk_icdGetInstanceProcAddr\n"); return 1; }
  PFN_vkVoidFunction (*neg)(uint32_t*) = dlsym(h, "vk_icdNegotiateLoaderICDInterfaceVersion");
  if (neg) { uint32_t v = 5; ((VkResult(*)(uint32_t*))neg)(&v); printf("icd iface %u\n", v); }
  L(NULL, vkCreateInstance);
  VkApplicationInfo ai = { VK_STRUCTURE_TYPE_APPLICATION_INFO, .pApplicationName = "vkinfo", .apiVersion = VK_API_VERSION_1_3 };
  VkInstanceCreateInfo ici = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &ai };
  VkInstance inst; VkResult r = vkCreateInstance(&ici, NULL, &inst);
  printf("vkCreateInstance = %d\n", r); if (r) return 1;
  L(inst, vkEnumeratePhysicalDevices); L(inst, vkGetPhysicalDeviceProperties2);
  L(inst, vkGetPhysicalDeviceFeatures2); L(inst, vkEnumerateDeviceExtensionProperties);
  L(inst, vkGetPhysicalDeviceMemoryProperties); L(inst, vkGetPhysicalDeviceFormatProperties);
  uint32_t n = 1; VkPhysicalDevice pd; r = vkEnumeratePhysicalDevices(inst, &n, &pd);
  printf("enum = %d n=%u\n", r, n); if (r || !n) return 1;
  VkPhysicalDeviceDriverProperties drv = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES };
  VkPhysicalDeviceProperties2 p2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &drv };
  vkGetPhysicalDeviceProperties2(pd, &p2);
  VkPhysicalDeviceProperties *p = &p2.properties;
  printf("device: %s api %u.%u.%u driver: %s / %s\n", p->deviceName, VK_VERSION_MAJOR(p->apiVersion),
         VK_VERSION_MINOR(p->apiVersion), VK_VERSION_PATCH(p->apiVersion), drv.driverName, drv.driverInfo);
  VkPhysicalDeviceVulkan13Features f13 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
  VkPhysicalDeviceVulkan12Features f12 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, &f13 };
  VkPhysicalDeviceVulkan11Features f11 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES, &f12 };
  VkPhysicalDeviceRobustness2FeaturesEXT rb = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT, &f11 };
  VkPhysicalDeviceTransformFeedbackFeaturesEXT xfb = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT, &rb };
  VkPhysicalDeviceCustomBorderColorFeaturesEXT cbc = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT, &xfb };
  VkPhysicalDeviceFeatures2 f2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &cbc };
  vkGetPhysicalDeviceFeatures2(pd, &f2);
  VkPhysicalDeviceFeatures *f = &f2.features;
#define F(s, x) printf("  %-40s %d\n", #x, (s)->x)
  F(f, geometryShader); F(f, tessellationShader); F(f, depthBounds); F(f, multiViewport); F(f, textureCompressionBC);
  F(f, textureCompressionETC2); F(f, textureCompressionASTC_LDR); F(f, pipelineStatisticsQuery); F(f, vertexPipelineStoresAndAtomics);
  F(f, shaderClipDistance); F(f, shaderCullDistance); F(f, fillModeNonSolid); F(f, multiDrawIndirect); F(f, dualSrcBlend);
  F(f, variableMultisampleRate); F(f, shaderStorageImageExtendedFormats); F(f, occlusionQueryPrecise); F(f, shaderFloat64);
  F(&rb, robustBufferAccess2); F(&rb, robustImageAccess2); F(&rb, nullDescriptor);
  F(&xfb, transformFeedback); F(&xfb, geometryStreams); F(&cbc, customBorderColors); F(&cbc, customBorderColorWithoutFormat);
  F(&f11, shaderDrawParameters); F(&f12, timelineSemaphore); F(&f12, descriptorIndexing); F(&f12, bufferDeviceAddress);
  F(&f12, runtimeDescriptorArray); F(&f12, descriptorBindingPartiallyBound); F(&f12, shaderFloat16); F(&f12, samplerMirrorClampToEdge);
  F(&f12, vulkanMemoryModel); F(&f12, scalarBlockLayout); F(&f12, hostQueryReset); F(&f12, drawIndirectCount);
  F(&f13, dynamicRendering); F(&f13, synchronization2); F(&f13, maintenance4); F(&f13, shaderDemoteToHelperInvocation);
  F(&f13, subgroupSizeControl); F(&f13, computeFullSubgroups);
  VkPhysicalDeviceMemoryProperties mp; vkGetPhysicalDeviceMemoryProperties(pd, &mp);
  for (uint32_t i = 0; i < mp.memoryHeapCount; i++) printf("heap %u size %llu MiB flags %x\n", i, (unsigned long long)(mp.memoryHeaps[i].size >> 20), mp.memoryHeaps[i].flags);
  for (uint32_t i = 0; i < mp.memoryTypeCount; i++) printf("type %u heap %u flags %x\n", i, mp.memoryTypes[i].heapIndex, mp.memoryTypes[i].propertyFlags);
  VkFormatProperties fp; vkGetPhysicalDeviceFormatProperties(pd, VK_FORMAT_BC1_RGBA_UNORM_BLOCK, &fp);
  printf("BC1 optimal features %x\n", fp.optimalTilingFeatures);
  vkGetPhysicalDeviceFormatProperties(pd, VK_FORMAT_BC3_UNORM_BLOCK, &fp); printf("BC3 optimal features %x\n", fp.optimalTilingFeatures);
  vkGetPhysicalDeviceFormatProperties(pd, VK_FORMAT_BC7_UNORM_BLOCK, &fp); printf("BC7 optimal features %x\n", fp.optimalTilingFeatures);
  uint32_t ne = 0; vkEnumerateDeviceExtensionProperties(pd, NULL, &ne, NULL);
  VkExtensionProperties *e = calloc(ne, sizeof(*e)); vkEnumerateDeviceExtensionProperties(pd, NULL, &ne, e);
  printf("%u device extensions:\n", ne); for (uint32_t i = 0; i < ne; i++) printf("  %s\n", e[i].extensionName);
  return 0;
}
