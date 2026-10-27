/* halshim: exposes an Android hwvulkan HAL module (vendor Vulkan driver) as a plain
 * ICD (vk_icdGetInstanceProcAddr) so the test harness can load it. HALSHIM_LIB =
 * module path (default /vendor/lib64/hw/vulkan.kirin990.so). */
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <vulkan/vulkan.h>

struct hw_module_t;
struct hw_device_t {
   uint32_t tag, version;
   struct hw_module_t *module;
   uint64_t reserved[12];
   int (*close)(struct hw_device_t *);
};
struct hw_module_methods_t {
   int (*open)(const struct hw_module_t *, const char *, struct hw_device_t **);
};
struct hw_module_t {
   uint32_t tag;
   uint16_t module_api_version, hal_api_version;
   const char *id, *name, *author;
   struct hw_module_methods_t *methods;
   void *dso;
   uint64_t reserved[32 - 7];
};
typedef struct {
   struct hw_device_t common;
   PFN_vkEnumerateInstanceExtensionProperties EnumerateInstanceExtensionProperties;
   PFN_vkCreateInstance CreateInstance;
   PFN_vkGetInstanceProcAddr GetInstanceProcAddr;
} hwvulkan_device_t;

static hwvulkan_device_t *hal;

static void load(void)
{
   if (hal) return;
   const char *p = getenv("HALSHIM_LIB");
   void *h = dlopen(p ? p : "/vendor/lib64/hw/vulkan.kirin990.so", RTLD_NOW);
   if (!h) { fprintf(stderr, "halshim: %s\n", dlerror()); abort(); }
   struct hw_module_t *m = dlsym(h, "HMI");
   if (!m || m->methods->open(m, "vk0", (struct hw_device_t **)&hal)) {
      fprintf(stderr, "halshim: open failed\n"); abort();
   }
}

__attribute__((visibility("default"))) PFN_vkVoidFunction
vk_icdGetInstanceProcAddr(VkInstance inst, const char *name)
{
   load();
   if (!inst) {
      if (!strcmp(name, "vkCreateInstance")) return (PFN_vkVoidFunction)hal->CreateInstance;
      if (!strcmp(name, "vkEnumerateInstanceExtensionProperties"))
         return (PFN_vkVoidFunction)hal->EnumerateInstanceExtensionProperties;
   }
   return hal->GetInstanceProcAddr(inst, name);
}
