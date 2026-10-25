/* Winlator-compositor-like: compute imageStore into a VkNativeBufferANDROID
 * swapchain image (1080x2340), sampling an imported AHB texture. */
#define VK_USE_PLATFORM_ANDROID_KHR 1
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vulkan/vk_android_native_buffer.h>
#include <android/hardware_buffer.h>
#include <cutils/native_handle.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include "c_spv.h"
const native_handle_t *AHardwareBuffer_getNativeHandle(const AHardwareBuffer *);
static PFN_vkVoidFunction (*gipa)(VkInstance, const char *);
#define L(i, n) PFN_##n n = (PFN_##n)gipa(i, #n)
#define D(n) PFN_##n n = (PFN_##n)gd(dev, #n)
#define CK(e, w) do { VkResult r_ = (e); if (r_) { printf("FAIL %s r=%d\n", w, r_); return 1; } } while (0)
#define DW 1080
#define DH 2340
#define SW 1172
#define SH 694
int main(int c, char **v) {
  setvbuf(stdout, NULL, _IONBF, 0);
  void *h = dlopen(v[1], RTLD_NOW); gipa = dlsym(h, "vk_icdGetInstanceProcAddr");
  L(NULL, vkCreateInstance);
  VkApplicationInfo ai = {VK_STRUCTURE_TYPE_APPLICATION_INFO, .apiVersion = VK_API_VERSION_1_1};
  VkInstanceCreateInfo ici = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &ai}; VkInstance in; vkCreateInstance(&ici, NULL, &in);
  L(in, vkEnumeratePhysicalDevices); L(in, vkCreateDevice); L(in, vkGetDeviceProcAddr); L(in, vkGetPhysicalDeviceMemoryProperties);
  uint32_t n = 1; VkPhysicalDevice pd; vkEnumeratePhysicalDevices(in, &n, &pd);
  float pr = 1; VkDeviceQueueCreateInfo q = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueCount = 1, .pQueuePriorities = &pr};
  const char *ex[] = {"VK_ANDROID_native_buffer", "VK_ANDROID_external_memory_android_hardware_buffer", "VK_KHR_external_memory", "VK_EXT_queue_family_foreign", "VK_KHR_sampler_ycbcr_conversion"};
  VkDeviceCreateInfo dci = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .queueCreateInfoCount = 1, .pQueueCreateInfos = &q, .enabledExtensionCount = 5, .ppEnabledExtensionNames = ex};
  VkDevice dev; CK(vkCreateDevice(pd, &dci, NULL, &dev), "dev"); PFN_vkGetDeviceProcAddr gd = vkGetDeviceProcAddr;
  D(vkGetDeviceQueue); D(vkCreateImage); D(vkCreateImageView); D(vkCreateSampler); D(vkCreateDescriptorSetLayout); D(vkCreatePipelineLayout);
  D(vkCreateShaderModule); D(vkCreateComputePipelines); D(vkCreateDescriptorPool); D(vkAllocateDescriptorSets); D(vkUpdateDescriptorSets);
  D(vkCreateCommandPool); D(vkAllocateCommandBuffers); D(vkBeginCommandBuffer); D(vkEndCommandBuffer); D(vkCmdBindPipeline);
  D(vkCmdBindDescriptorSets); D(vkCmdDispatch); D(vkCmdPipelineBarrier); D(vkQueueSubmit); D(vkQueueWaitIdle);
  D(vkGetAndroidHardwareBufferPropertiesANDROID); D(vkAllocateMemory); D(vkBindImageMemory); D(vkCmdClearColorImage);
  VkQueue qu; vkGetDeviceQueue(dev, 0, 0, &qu);
  /* dst: swapchain-like gralloc buffer through VkNativeBufferANDROID */
  AHardwareBuffer_Desc dd = {DW, DH, 1, 1, USAGE_DST}; AHardwareBuffer *dahb; if (AHardwareBuffer_allocate(&dd, &dahb)) { printf("FAIL alloc dst\n"); return 1; }
  AHardwareBuffer_describe(dahb, &dd);
  VkNativeBufferANDROID nb = {VK_STRUCTURE_TYPE_NATIVE_BUFFER_ANDROID, .handle = AHardwareBuffer_getNativeHandle(dahb), .stride = dd.stride, .format = 1, .usage = (int)USAGE_DST};
  VkImageCreateInfo di = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, &nb, 0, VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, {DW, DH, 1}, 1, 1, 1, VK_IMAGE_TILING_OPTIMAL,
    VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, 0, 0, NULL, VK_IMAGE_LAYOUT_UNDEFINED};
  VkImage dimg; CK(vkCreateImage(dev, &di, NULL, &dimg), "dst image");
  /* src: game window pixmap, imported AHB, sampled */
  AHardwareBuffer_Desc sd = {SW, SH, 1, 1, 0x300}; AHardwareBuffer *sahb; AHardwareBuffer_allocate(&sd, &sahb);
  { void *p; AHardwareBuffer_lock(sahb, 0x30, -1, NULL, &p); AHardwareBuffer_describe(sahb, &sd); for (uint32_t y = 0; y < SH; y++) for (uint32_t x = 0; x < SW; x++) ((uint32_t *)p)[y * sd.stride + x] = 0xff00ff00; AHardwareBuffer_unlock(sahb, NULL); }
  VkAndroidHardwareBufferFormatPropertiesANDROID fp = {VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID};
  VkAndroidHardwareBufferPropertiesANDROID ap = {VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID, &fp};
  CK(vkGetAndroidHardwareBufferPropertiesANDROID(dev, sahb, &ap), "ahb props");
  VkExternalMemoryImageCreateInfo emi = {VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO, .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID};
  VkImageCreateInfo si = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, &emi, 0, VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, {SW, SH, 1}, 1, 1, 1, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT, 0, 0, NULL, VK_IMAGE_LAYOUT_UNDEFINED};
  VkImage simg; CK(vkCreateImage(dev, &si, NULL, &simg), "src image");
  VkImportAndroidHardwareBufferInfoANDROID imp = {VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID, .buffer = sahb};
  VkMemoryDedicatedAllocateInfo ded = {VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO, &imp, simg};
  VkMemoryAllocateInfo ma = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, &ded, ap.allocationSize, __builtin_ctz(ap.memoryTypeBits)};
  VkDeviceMemory sm; CK(vkAllocateMemory(dev, &ma, NULL, &sm), "src import"); CK(vkBindImageMemory(dev, simg, sm, 0), "src bind");
  VkImageViewCreateInfo vc = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = dimg, .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = VK_FORMAT_R8G8B8A8_UNORM, .subresourceRange = {1, 0, 1, 0, 1}};
  VkImageView dv, sv; CK(vkCreateImageView(dev, &vc, NULL, &dv), "dv"); vc.image = simg; CK(vkCreateImageView(dev, &vc, NULL, &sv), "sv");
  VkSamplerCreateInfo sc = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .magFilter = VK_FILTER_LINEAR, .minFilter = VK_FILTER_LINEAR}; VkSampler smp; vkCreateSampler(dev, &sc, NULL, &smp);
  VkDescriptorSetLayoutBinding b[2] = {{0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT}, {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT}};
  VkDescriptorSetLayoutCreateInfo lc = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = 2, .pBindings = b}; VkDescriptorSetLayout dsl; vkCreateDescriptorSetLayout(dev, &lc, NULL, &dsl);
  VkPipelineLayoutCreateInfo plc = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .setLayoutCount = 1, .pSetLayouts = &dsl}; VkPipelineLayout pl; vkCreatePipelineLayout(dev, &plc, NULL, &pl);
  VkShaderModuleCreateInfo smc = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = c_spv_len, .pCode = (const uint32_t *)c_spv}; VkShaderModule mod; vkCreateShaderModule(dev, &smc, NULL, &mod);
  VkComputePipelineCreateInfo cpc = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, .stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = mod, .pName = "main"}, .layout = pl};
  VkPipeline pipe; CK(vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &cpc, NULL, &pipe), "pipe");
  VkDescriptorPoolSize ps[2] = {{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}, {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}};
  VkDescriptorPoolCreateInfo dpc = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .maxSets = 1, .poolSizeCount = 2, .pPoolSizes = ps}; VkDescriptorPool dp; vkCreateDescriptorPool(dev, &dpc, NULL, &dp);
  VkDescriptorSetAllocateInfo dsa = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, .descriptorPool = dp, .descriptorSetCount = 1, .pSetLayouts = &dsl}; VkDescriptorSet ds; vkAllocateDescriptorSets(dev, &dsa, &ds);
  VkDescriptorImageInfo ii[2] = {{0, dv, VK_IMAGE_LAYOUT_GENERAL}, {smp, sv, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}};
  VkWriteDescriptorSet w[2] = {{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = ds, .dstBinding = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &ii[0]},
                               {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = ds, .dstBinding = 1, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &ii[1]}};
  vkUpdateDescriptorSets(dev, 2, w, 0, NULL);
  VkCommandPoolCreateInfo cp = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO}; VkCommandPool pool; vkCreateCommandPool(dev, &cp, NULL, &pool);
  VkCommandBufferAllocateInfo ca = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = pool, .commandBufferCount = 1}; VkCommandBuffer cb; vkAllocateCommandBuffers(dev, &ca, &cb);
  VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; vkBeginCommandBuffer(cb, &bi);
  VkImageMemoryBarrier bar[2] = {{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT, .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED, .newLayout = VK_IMAGE_LAYOUT_GENERAL, .srcQueueFamilyIndex = ~0u, .dstQueueFamilyIndex = ~0u, .image = dimg, .subresourceRange = {1, 0, 1, 0, 1}},
    {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, .dstAccessMask = VK_ACCESS_SHADER_READ_BIT, .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED, .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, .srcQueueFamilyIndex = VK_QUEUE_FAMILY_FOREIGN_EXT, .dstQueueFamilyIndex = 0, .image = simg, .subresourceRange = {1, 0, 1, 0, 1}}};
  vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 2, bar);
  vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipe); vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pl, 0, 1, &ds, 0, NULL);
  vkCmdDispatch(cb, (DW + 7) / 8, (DH + 7) / 8, 1); CK(vkEndCommandBuffer(cb), "end");
  VkSubmitInfo s = {VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cb};
  for (int i = 0; i < 5; i++) { CK(vkQueueSubmit(qu, 1, &s, VK_NULL_HANDLE), "submit"); CK(vkQueueWaitIdle(qu), "idle"); }
  void *p; AHardwareBuffer_lock(dahb, 0x3, -1, NULL, &p);
  uint32_t a = ((uint32_t *)p)[10 * dd.stride + 10], z = ((uint32_t *)p)[(DH - 3) * dd.stride + DW - 3];
  printf("dst px %08x %08x (want ~ff80ff80 = R half + src green) %s\n", a, z, (a == z && (a & 0xff00) > 0x7000 && (a & 0xff) > 0x70) ? "PASS" : "FAIL");
  AHardwareBuffer_unlock(dahb, NULL);
  return 0;
}
