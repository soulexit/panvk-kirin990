#define VK_USE_PLATFORM_ANDROID_KHR 1
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vulkan/vk_android_native_buffer.h>
#include <dlfcn.h>
#include <stdio.h>
int main(int c, char **v) {
  void *h = dlopen(v[1], RTLD_NOW); PFN_vkVoidFunction (*g)(VkInstance, const char *) = dlsym(h, "vk_icdGetInstanceProcAddr");
  PFN_vkCreateInstance ci = (void *)g(NULL, "vkCreateInstance");
  VkApplicationInfo ai = {VK_STRUCTURE_TYPE_APPLICATION_INFO, .apiVersion = VK_API_VERSION_1_1};
  VkInstanceCreateInfo ici = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &ai}; VkInstance in; ci(&ici, NULL, &in);
  PFN_vkEnumeratePhysicalDevices ep = (void *)g(in, "vkEnumeratePhysicalDevices"); uint32_t n = 1; VkPhysicalDevice pd; ep(in, &n, &pd);
  PFN_vkCreateDevice cd = (void *)g(in, "vkCreateDevice"); PFN_vkGetDeviceProcAddr gd = (void *)g(in, "vkGetDeviceProcAddr");
  float pr = 1; VkDeviceQueueCreateInfo q = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueCount = 1, .pQueuePriorities = &pr};
  const char *ex[] = {"VK_ANDROID_native_buffer"};
  VkDeviceCreateInfo dci = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .queueCreateInfoCount = 1, .pQueueCreateInfos = &q, .enabledExtensionCount = 1, .ppEnabledExtensionNames = ex};
  VkDevice d; printf("dev %d\n", cd(pd, &dci, NULL, &d));
  PFN_vkGetSwapchainGrallocUsage2ANDROID u2 = (void *)gd(d, "vkGetSwapchainGrallocUsage2ANDROID");
  PFN_vkGetSwapchainGrallocUsageANDROID u1 = (void *)gd(d, "vkGetSwapchainGrallocUsageANDROID");
  uint64_t cu = 0, pu = 0; int g0 = 0;
  if (u2) printf("usage2 r=%d consumer=%llx producer=%llx\n", u2(d, VK_FORMAT_R8G8B8A8_UNORM, 0x13, 0, &cu, &pu), (unsigned long long)cu, (unsigned long long)pu);
  if (u1) printf("usage1 r=%d gralloc0=%x\n", u1(d, VK_FORMAT_R8G8B8A8_UNORM, 0x13, &g0), g0);
}
