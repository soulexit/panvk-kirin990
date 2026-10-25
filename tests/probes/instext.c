#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <dlfcn.h>
#include <stdio.h>
int main(int c, char **v){void*h=dlopen(v[1],RTLD_NOW);PFN_vkVoidFunction(*g)(VkInstance,const char*)=dlsym(h,"vk_icdGetInstanceProcAddr");
PFN_vkEnumerateInstanceExtensionProperties e=(void*)g(NULL,"vkEnumerateInstanceExtensionProperties");uint32_t n=64;VkExtensionProperties p[64];e(NULL,&n,p);for(uint32_t i=0;i<n;i++)printf("%s\n",p[i].extensionName);}
