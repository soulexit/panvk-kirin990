#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <dlfcn.h>
#include <stdio.h>
int main(int c, char **v) {
  void *h = dlopen(v[1], RTLD_NOW); PFN_vkVoidFunction (*g)(VkInstance, const char *) = dlsym(h, "vk_icdGetInstanceProcAddr");
  PFN_vkCreateInstance ci = (void *)g(NULL, "vkCreateInstance");
  VkApplicationInfo ai = {VK_STRUCTURE_TYPE_APPLICATION_INFO, .apiVersion = VK_API_VERSION_1_3};
  VkInstanceCreateInfo ici = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &ai}; VkInstance in; ci(&ici, NULL, &in);
  PFN_vkEnumeratePhysicalDevices ep = (void *)g(in, "vkEnumeratePhysicalDevices"); uint32_t n = 1; VkPhysicalDevice pd; ep(in, &n, &pd);
  PFN_vkGetPhysicalDeviceProperties2 gp = (void *)g(in, "vkGetPhysicalDeviceProperties2");
  VkPhysicalDeviceSubgroupProperties sg = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
  VkPhysicalDeviceProperties2 p = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &sg}; gp(pd, &p);
  VkPhysicalDeviceLimits *l = &p.properties.limits;
  printf("maxComputeWorkGroupInvocations=%u size=%ux%ux%u shared=%u subgroup=%u\n", l->maxComputeWorkGroupInvocations,
         l->maxComputeWorkGroupSize[0], l->maxComputeWorkGroupSize[1], l->maxComputeWorkGroupSize[2], l->maxComputeSharedMemorySize, sg.subgroupSize);
}
