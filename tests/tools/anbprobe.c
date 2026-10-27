/* anbprobe <icd.so> : call the driver's vkQueueSignalReleaseImageANDROID like the
 * Android swapchain does at present time (one wait semaphore signalled by a
 * submit), and report each step: semaphore export support, the submit, and the
 * returned native fence fd. Winlator's Vulkan compositor ("Renderer Driver")
 * fails every present at this call with -13. */
#define VK_NO_PROTOTYPES
#define VK_USE_PLATFORM_ANDROID_KHR
#include <vulkan/vulkan.h>
typedef VkResult (VKAPI_PTR *PFN_vkQueueSignalReleaseImageANDROID)(
   VkQueue queue, uint32_t waitSemaphoreCount, const VkSemaphore *pWaitSemaphores,
   VkImage image, int *pNativeFenceFd);
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static PFN_vkGetInstanceProcAddr gipa;
static VkInstance inst;
static VkDevice dev;
#define I(n) PFN_##n n = (PFN_##n)gipa(inst, #n)
#define D(n) PFN_##n n = (PFN_##n)gdpa(dev, #n)

int main(int argc, char **argv)
{
   void *h = dlopen(argv[1], RTLD_NOW);
   if (!h) { printf("dlopen %s\n", dlerror()); return 1; }
   gipa = (PFN_vkGetInstanceProcAddr)dlsym(h, "vk_icdGetInstanceProcAddr");
   I(vkCreateInstance);
   VkApplicationInfo ai = {VK_STRUCTURE_TYPE_APPLICATION_INFO, .apiVersion = VK_API_VERSION_1_3};
   VkInstanceCreateInfo ici = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &ai};
   printf("vkCreateInstance %d\n", vkCreateInstance(&ici, NULL, &inst));
   I(vkEnumeratePhysicalDevices); I(vkCreateDevice); I(vkGetDeviceProcAddr);
   I(vkGetPhysicalDeviceExternalSemaphoreProperties);
   PFN_vkGetDeviceProcAddr gdpa = vkGetDeviceProcAddr;
   uint32_t n = 1; VkPhysicalDevice pd; vkEnumeratePhysicalDevices(inst, &n, &pd);

   VkPhysicalDeviceExternalSemaphoreInfo esi = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO,
      .handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT};
   VkExternalSemaphoreProperties esp = {VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES};
   vkGetPhysicalDeviceExternalSemaphoreProperties(pd, &esi, &esp);
   printf("SYNC_FD semaphore: exportable %d importable %d (features 0x%x)\n",
          !!(esp.externalSemaphoreFeatures & VK_EXTERNAL_SEMAPHORE_FEATURE_EXPORTABLE_BIT),
          !!(esp.externalSemaphoreFeatures & VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT),
          esp.externalSemaphoreFeatures);

   const char *exts[] = {"VK_ANDROID_native_buffer", "VK_KHR_external_semaphore_fd"};
   float prio = 1;
   VkDeviceQueueCreateInfo qci = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueCount = 1,
                                  .pQueuePriorities = &prio};
   VkDeviceCreateInfo dci = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .queueCreateInfoCount = 1,
                             .pQueueCreateInfos = &qci, .enabledExtensionCount = 2,
                             .ppEnabledExtensionNames = exts};
   VkResult r = vkCreateDevice(pd, &dci, NULL, &dev);
   printf("vkCreateDevice (VK_ANDROID_native_buffer) %d\n", r);
   if (r) return 1;
   D(vkGetDeviceQueue); D(vkCreateSemaphore); D(vkQueueSubmit); D(vkQueueWaitIdle);
   D(vkQueueSignalReleaseImageANDROID); D(vkGetSemaphoreFdKHR);
   printf("vkQueueSignalReleaseImageANDROID %p\n", (void *)vkQueueSignalReleaseImageANDROID);
   VkQueue q; vkGetDeviceQueue(dev, 0, 0, &q);

   /* The frame's semaphore, signalled by the compositor's render submit. */
   VkSemaphoreCreateInfo sci = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
   VkSemaphore frame; vkCreateSemaphore(dev, &sci, NULL, &frame);
   VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO, .signalSemaphoreCount = 1,
                      .pSignalSemaphores = &frame};
   printf("submit signalling frame semaphore %d\n", vkQueueSubmit(q, 1, &si, VK_NULL_HANDLE));

   for (int i = 0; i < 3; i++) {
      int fd = -2;
      r = vkQueueSignalReleaseImageANDROID(q, 1, &frame, VK_NULL_HANDLE, &fd);
      printf("vkQueueSignalReleaseImageANDROID #%d -> %d, fd %d\n", i, r, fd);
      if (fd >= 0) close(fd);
      vkQueueSubmit(q, 1, &si, VK_NULL_HANDLE);
   }

   /* The export on its own, from an exportable semaphore. */
   VkExportSemaphoreCreateInfo exi = {VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO,
                                      .handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT};
   VkSemaphoreCreateInfo sci2 = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, &exi};
   VkSemaphore ex;
   printf("create exportable semaphore %d\n", vkCreateSemaphore(dev, &sci2, NULL, &ex));
   VkSubmitInfo si2 = {VK_STRUCTURE_TYPE_SUBMIT_INFO, .signalSemaphoreCount = 1,
                       .pSignalSemaphores = &ex};
   printf("submit signalling it %d\n", vkQueueSubmit(q, 1, &si2, VK_NULL_HANDLE));
   VkSemaphoreGetFdInfoKHR gfi = {VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR,
                                  .semaphore = ex,
                                  .handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT};
   int fd = -2;
   r = vkGetSemaphoreFdKHR(dev, &gfi, &fd);
   printf("vkGetSemaphoreFdKHR(SYNC_FD) -> %d, fd %d\n", r, fd);
   vkQueueWaitIdle(q);
   return 0;
}
