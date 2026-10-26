/* drmprobe <icd.so> [vkformat] : like dEQP-VK.drm_format_modifiers.create_list_modifiers —
 * query the format's DRM modifiers, create an image with the whole list, query the chosen one. */
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <vulkan/vulkan.h>

static PFN_vkGetInstanceProcAddr gipa;
static VkInstance inst;
#define L(n) PFN_##n n = (PFN_##n)gipa(inst, #n)

int main(int argc, char **argv)
{
   void *h = dlopen(argv[1], RTLD_NOW);
   if (!h) { printf("dlopen: %s\n", dlerror()); return 1; }
   gipa = (PFN_vkGetInstanceProcAddr)dlsym(h, "vk_icdGetInstanceProcAddr");
   VkFormat fmt = argc > 2 ? (VkFormat)atoi(argv[2]) : VK_FORMAT_B8G8R8A8_SRGB;
   L(vkCreateInstance);
   VkApplicationInfo ai = {VK_STRUCTURE_TYPE_APPLICATION_INFO, .apiVersion = VK_API_VERSION_1_3};
   VkInstanceCreateInfo ici = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &ai};
   if (vkCreateInstance(&ici, NULL, &inst)) return 1;
   L(vkEnumeratePhysicalDevices); L(vkGetPhysicalDeviceFormatProperties2); L(vkCreateDevice);
   L(vkGetDeviceProcAddr); L(vkGetPhysicalDeviceImageFormatProperties2);
   uint32_t n = 1; VkPhysicalDevice pd; vkEnumeratePhysicalDevices(inst, &n, &pd);

   VkDrmFormatModifierPropertiesListEXT ml = {VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT};
   VkFormatProperties2 fp = {VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2, &ml};
   vkGetPhysicalDeviceFormatProperties2(pd, fmt, &fp);
   VkDrmFormatModifierPropertiesEXT *mp = calloc(ml.drmFormatModifierCount, sizeof(*mp));
   ml.pDrmFormatModifierProperties = mp;
   vkGetPhysicalDeviceFormatProperties2(pd, fmt, &fp);
   printf("%u modifiers\n", ml.drmFormatModifierCount);
   uint64_t *mods = calloc(ml.drmFormatModifierCount, 8);
   uint32_t nm = 0;
   for (uint32_t i = 0; i < ml.drmFormatModifierCount; i++) {
      printf("  0x%016llx planes=%u feats=0x%x\n", (unsigned long long)mp[i].drmFormatModifier,
             mp[i].drmFormatModifierPlaneCount, mp[i].drmFormatModifierTilingFeatures);
      VkPhysicalDeviceExternalImageFormatInfo ei = {
         VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO, NULL,
         VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT};
      VkPhysicalDeviceImageDrmFormatModifierInfoEXT mi = {
         VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT, &ei,
         .drmFormatModifier = mp[i].drmFormatModifier};
      VkPhysicalDeviceImageFormatInfo2 ifi = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2, &mi,
         fmt, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT,
         VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT};
      VkExternalImageFormatProperties eip = {VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES};
      VkImageFormatProperties2 ifp = {VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2, &eip};
      VkResult r = vkGetPhysicalDeviceImageFormatProperties2(pd, &ifi, &ifp);
      printf("    image props %d\n", r);
      if (!r) mods[nm++] = mp[i].drmFormatModifier;
   }

   float prio = 1;
   const char *ext[] = {"VK_EXT_image_drm_format_modifier"};
   VkDeviceQueueCreateInfo qci = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueCount = 1,
                                  .pQueuePriorities = &prio};
   VkDeviceCreateInfo dci = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .queueCreateInfoCount = 1,
                             .pQueueCreateInfos = &qci, .enabledExtensionCount = 1,
                             .ppEnabledExtensionNames = ext};
   VkDevice dev;
   if (vkCreateDevice(pd, &dci, NULL, &dev)) return 1;
   PFN_vkCreateImage vkCreateImage = (PFN_vkCreateImage)vkGetDeviceProcAddr(dev, "vkCreateImage");
   PFN_vkGetImageDrmFormatModifierPropertiesEXT getmod = (PFN_vkGetImageDrmFormatModifierPropertiesEXT)
      vkGetDeviceProcAddr(dev, "vkGetImageDrmFormatModifierPropertiesEXT");
   printf("vkGetImageDrmFormatModifierPropertiesEXT = %p\n", (void *)getmod);
   for (uint32_t len = 1; len <= nm; len++) {
      VkExternalMemoryImageCreateInfo emi = {VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO, NULL,
                                             VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT};
      VkImageDrmFormatModifierListCreateInfoEXT lci = {
         VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT, &emi,
         .drmFormatModifierCount = len, .pDrmFormatModifiers = mods};
      VkImageFormatListCreateInfo fli = {VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO, &lci, 1, &fmt};
      VkImageCreateInfo imci = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, &fli, .imageType = VK_IMAGE_TYPE_2D,
         .format = fmt, .extent = {64, 64, 1}, .mipLevels = 1, .arrayLayers = 1, .samples = 1,
         .tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT,
         .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT};
      VkImage img;
      printf("create image with %u modifiers...\n", len); fflush(stdout);
      VkResult r = vkCreateImage(dev, &imci, NULL, &img);
      printf("vkCreateImage = %d\n", r); fflush(stdout);
      if (r) return 1;
      VkImageDrmFormatModifierPropertiesEXT ip = {VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_PROPERTIES_EXT};
      r = getmod(dev, img, &ip);
      printf("chosen modifier %d 0x%llx\n", r, (unsigned long long)ip.drmFormatModifier); fflush(stdout);
   }
   return 0;
}
