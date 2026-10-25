/* Compare what a WSI layer sees for AHB-backed BGRA/RGBA swapchain images. */
#define VK_USE_PLATFORM_ANDROID_KHR 1
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <android/hardware_buffer.h>
#include <dlfcn.h>
#include <stdio.h>
static PFN_vkVoidFunction (*gipa)(VkInstance, const char *);
#define L(i, n) PFN_##n n = (PFN_##n)gipa(i, #n)
int main(int c, char **v) {
  setvbuf(stdout, NULL, _IONBF, 0);
  void *h = dlopen(v[1], RTLD_NOW); gipa = dlsym(h, "vk_icdGetInstanceProcAddr");
  L(NULL, vkCreateInstance);
  VkApplicationInfo ai = {VK_STRUCTURE_TYPE_APPLICATION_INFO, .apiVersion = VK_API_VERSION_1_3};
  VkInstanceCreateInfo ici = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &ai};
  VkInstance in; vkCreateInstance(&ici, NULL, &in);
  L(in, vkEnumeratePhysicalDevices); L(in, vkGetPhysicalDeviceImageFormatProperties2); L(in, vkCreateDevice); L(in, vkGetDeviceProcAddr);
  L(in, vkGetPhysicalDeviceFormatProperties2);
  uint32_t n = 1; VkPhysicalDevice pd; vkEnumeratePhysicalDevices(in, &n, &pd);
  VkFormat fmts[] = {VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8A8_SRGB};
  VkImageUsageFlags us[] = {0x13 /*xfer src|dst|color*/, 0x17, 0x1f, 0x10, 0x9f};
  for (int f = 0; f < 3; f++) {
    VkFormatProperties3 fp3 = {VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_3};
    VkFormatProperties2 fp = {VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2, &fp3};
    vkGetPhysicalDeviceFormatProperties2(pd, fmts[f], &fp);
    printf("fmt %d lin=%llx opt=%llx\n", fmts[f], (unsigned long long)fp3.linearTilingFeatures, (unsigned long long)fp3.optimalTilingFeatures);
    for (int t = 0; t < 2; t++) for (int u = 0; u < 5; u++) {
      VkPhysicalDeviceExternalImageFormatInfo ext = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO, .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID};
      VkPhysicalDeviceImageFormatInfo2 ifi = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2, &ext, fmts[f], VK_IMAGE_TYPE_2D, t ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL, us[u], 0};
      VkAndroidHardwareBufferUsageANDROID au = {VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_USAGE_ANDROID};
      VkExternalImageFormatProperties ep = {VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES, &au};
      VkImageFormatProperties2 ip = {VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2, &ep};
      VkResult r = vkGetPhysicalDeviceImageFormatProperties2(pd, &ifi, &ip);
      printf("  tiling=%d usage=%x r=%d extfeat=%x compat=%x ahbusage=%llx maxext=%u\n", t, us[u], r, ep.externalMemoryProperties.externalMemoryFeatures, ep.externalMemoryProperties.compatibleHandleTypes, (unsigned long long)au.androidHardwareBufferUsage, ip.imageFormatProperties.maxExtent.width);
    }
  }
  float pr = 1; VkDeviceQueueCreateInfo q = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueCount = 1, .pQueuePriorities = &pr};
  const char *ex[] = {"VK_ANDROID_external_memory_android_hardware_buffer", "VK_KHR_external_memory", "VK_EXT_queue_family_foreign"};
  VkDeviceCreateInfo dci = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .queueCreateInfoCount = 1, .pQueueCreateInfos = &q, .enabledExtensionCount = 3, .ppEnabledExtensionNames = ex};
  VkDevice d; printf("createDevice %d\n", vkCreateDevice(pd, &dci, NULL, &d));
  PFN_vkGetAndroidHardwareBufferPropertiesANDROID gp = (void *)vkGetDeviceProcAddr(d, "vkGetAndroidHardwareBufferPropertiesANDROID");
  uint64_t aus[] = {AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT | AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE,
                    AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT | AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE | AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN | AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN,
                    AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE | AHARDWAREBUFFER_USAGE_COMPOSER_OVERLAY | AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT};
  for (int fm = 1; fm <= 5; fm += 4) for (int u = 0; u < 3; u++) {
    AHardwareBuffer_Desc desc = {964, 530, 1, fm, aus[u]}; AHardwareBuffer *ahb = NULL;
    if (AHardwareBuffer_allocate(&desc, &ahb)) { printf("alloc fail fmt %d\n", fm); continue; }
    AHardwareBuffer_Desc od; AHardwareBuffer_describe(ahb, &od);
    VkAndroidHardwareBufferFormatPropertiesANDROID fpr = {VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID};
    VkAndroidHardwareBufferPropertiesANDROID pr2 = {VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID, &fpr};
    VkResult r = gp(d, ahb, &pr2);
    printf("ahb fmt=%d usage=%llx stride=%u: r=%d size=%llu bits=%x vkfmt=%d extfmt=%llu feat=%x\n", fm, (unsigned long long)aus[u], od.stride, r, (unsigned long long)pr2.allocationSize, pr2.memoryTypeBits, fpr.format, (unsigned long long)fpr.externalFormat, fpr.formatFeatures);
    AHardwareBuffer_release(ahb);
  }
  return 0;
}
