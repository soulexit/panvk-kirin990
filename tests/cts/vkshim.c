/* Minimal stand-in for libvulkan.so: forwards to a Mesa ICD without the loader.
 * ICD path from $VKSHIM_ICD (default ./libvulkan_panfrost.so). */
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <vulkan/vulkan.h>

typedef PFN_vkVoidFunction (*gipa_t)(VkInstance, const char *);
typedef VkResult (*neg_t)(uint32_t *);
static gipa_t icd_gipa;

static void load(void)
{
   if (icd_gipa) return;
   const char *p = getenv("VKSHIM_ICD");
   void *h = dlopen(p ? p : "./libvulkan_panfrost.so", RTLD_NOW | RTLD_LOCAL);
   if (!h) { fprintf(stderr, "vkshim: %s\n", dlerror()); abort(); }
   neg_t neg = (neg_t)dlsym(h, "vk_icdNegotiateLoaderICDInterfaceVersion");
   uint32_t v = 5;
   if (neg) neg(&v);
   icd_gipa = (gipa_t)dlsym(h, "vk_icdGetInstanceProcAddr");
}

__attribute__((visibility("default"))) PFN_vkVoidFunction
vkGetInstanceProcAddr(VkInstance inst, const char *name)
{
   load();
   return icd_gipa(inst, name);
}
