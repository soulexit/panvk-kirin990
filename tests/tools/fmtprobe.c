/* fmtprobe <icd.so>: optimal-tiling features of the ASTC LDR formats, against
 * what yuzu-based emulators need to sample ASTC natively (else they decode
 * every ASTC texture with a compute shader). */
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <dlfcn.h>
#include <stdio.h>
int main(int argc, char **argv)
{
   void *h = dlopen(argv[1], RTLD_NOW);
   PFN_vkGetInstanceProcAddr gipa = (PFN_vkGetInstanceProcAddr)dlsym(h, "vk_icdGetInstanceProcAddr");
   PFN_vkCreateInstance ci = (PFN_vkCreateInstance)gipa(NULL, "vkCreateInstance");
   VkApplicationInfo ai = {VK_STRUCTURE_TYPE_APPLICATION_INFO, .apiVersion = VK_API_VERSION_1_1};
   VkInstanceCreateInfo ici = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &ai};
   VkInstance inst; ci(&ici, NULL, &inst);
   PFN_vkEnumeratePhysicalDevices ep = (PFN_vkEnumeratePhysicalDevices)gipa(inst, "vkEnumeratePhysicalDevices");
   PFN_vkGetPhysicalDeviceFormatProperties fp = (PFN_vkGetPhysicalDeviceFormatProperties)gipa(inst, "vkGetPhysicalDeviceFormatProperties");
   uint32_t n = 1; VkPhysicalDevice pd; ep(inst, &n, &pd);
   const VkFormatFeatureFlags want = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_BLIT_SRC_BIT |
      VK_FORMAT_FEATURE_BLIT_DST_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
   for (VkFormat f = VK_FORMAT_ASTC_4x4_UNORM_BLOCK; f <= VK_FORMAT_ASTC_12x12_SRGB_BLOCK; f++) {
      VkFormatProperties p; fp(pd, f, &p);
      VkFormatFeatureFlags o = p.optimalTilingFeatures;
      printf("fmt %u optimal %#x missing %#x%s\n", f, o, want & ~o, (want & ~o) & VK_FORMAT_FEATURE_BLIT_DST_BIT ? " (BLIT_DST)" : "");
   }
   return 0;
}
