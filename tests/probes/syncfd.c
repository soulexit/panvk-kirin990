/* Probe external sync FD support + import of signaled (-1) sync fds. */
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <dlfcn.h>
#include <stdio.h>
static PFN_vkVoidFunction (*gipa)(VkInstance, const char *);
#define L(i, n) PFN_##n n = (PFN_##n)gipa(i, #n)
int main(int c, char **v) {
  setvbuf(stdout, NULL, _IONBF, 0); void *h = dlopen(v[1], RTLD_NOW); gipa = dlsym(h, "vk_icdGetInstanceProcAddr");
  L(NULL, vkCreateInstance);
  VkApplicationInfo ai = {VK_STRUCTURE_TYPE_APPLICATION_INFO, .apiVersion = VK_API_VERSION_1_3};
  VkInstanceCreateInfo ici = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &ai};
  VkInstance in; vkCreateInstance(&ici, NULL, &in);
  L(in, vkEnumeratePhysicalDevices); L(in, vkGetPhysicalDeviceExternalSemaphoreProperties);
  L(in, vkGetPhysicalDeviceExternalFenceProperties); L(in, vkCreateDevice); L(in, vkGetDeviceProcAddr);
  uint32_t n = 1; VkPhysicalDevice pd; vkEnumeratePhysicalDevices(in, &n, &pd);
  VkExternalSemaphoreHandleTypeFlagBits st[] = {VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT};
  for (int i = 0; i < 2; i++) {
    VkPhysicalDeviceExternalSemaphoreInfo si = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO, .handleType = st[i]};
    VkExternalSemaphoreProperties sp = {VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES};
    vkGetPhysicalDeviceExternalSemaphoreProperties(pd, &si, &sp);
    printf("sem type %x: export %x compat %x features %x\n", st[i], sp.exportFromImportedHandleTypes, sp.compatibleHandleTypes, sp.externalSemaphoreFeatures);
    VkPhysicalDeviceExternalFenceInfo fi = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_FENCE_INFO, .handleType = st[i]};
    VkExternalFenceProperties fp = {VK_STRUCTURE_TYPE_EXTERNAL_FENCE_PROPERTIES};
    vkGetPhysicalDeviceExternalFenceProperties(pd, &fi, &fp);
    printf("fence type %x: compat %x features %x\n", st[i], fp.compatibleHandleTypes, fp.externalFenceFeatures);
  }
  float pr = 1; VkDeviceQueueCreateInfo q = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueCount = 1, .pQueuePriorities = &pr};
  const char *ex[] = {"VK_KHR_external_semaphore_fd", "VK_KHR_external_fence_fd"};
  VkDeviceCreateInfo dci = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .queueCreateInfoCount = 1, .pQueueCreateInfos = &q, .enabledExtensionCount = 2, .ppEnabledExtensionNames = ex};
  VkDevice d; VkResult r = vkCreateDevice(pd, &dci, NULL, &d); printf("createDevice %d\n", r);
#define D(nm) PFN_##nm nm = (PFN_##nm)vkGetDeviceProcAddr(d, #nm)
  D(vkCreateSemaphore); D(vkCreateFence); D(vkImportSemaphoreFdKHR); D(vkImportFenceFdKHR); D(vkWaitForFences);
  D(vkGetDeviceQueue); D(vkQueueSubmit); D(vkGetFenceFdKHR); D(vkGetSemaphoreFdKHR); D(vkQueueWaitIdle);
  VkSemaphoreCreateInfo sci = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO}; VkSemaphore s; vkCreateSemaphore(d, &sci, NULL, &s);
  VkImportSemaphoreFdInfoKHR is = {VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR, .semaphore = s, .flags = VK_SEMAPHORE_IMPORT_TEMPORARY_BIT, .handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT, .fd = -1};
  printf("import sem syncfd(-1) %d\n", vkImportSemaphoreFdKHR(d, &is));
  VkFenceCreateInfo fci = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO}; VkFence f; vkCreateFence(d, &fci, NULL, &f);
  VkImportFenceFdInfoKHR ifd = {VK_STRUCTURE_TYPE_IMPORT_FENCE_FD_INFO_KHR, .fence = f, .flags = VK_FENCE_IMPORT_TEMPORARY_BIT, .handleType = VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT, .fd = -1};
  printf("import fence syncfd(-1) %d\n", vkImportFenceFdKHR(d, &ifd));
  printf("wait imported fence %d\n", vkWaitForFences(d, 1, &f, 1, 1000000000ull));
  /* export path: submit signalling a fence + semaphore, export sync fds */
  VkQueue qu; vkGetDeviceQueue(d, 0, 0, &qu);
  VkExportFenceCreateInfo efc = {VK_STRUCTURE_TYPE_EXPORT_FENCE_CREATE_INFO, .handleTypes = VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT};
  fci.pNext = &efc; VkFence f2; printf("create export fence %d\n", vkCreateFence(d, &fci, NULL, &f2));
  VkExportSemaphoreCreateInfo esc = {VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO, .handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT};
  sci.pNext = &esc; VkSemaphore s2; printf("create export sem %d\n", vkCreateSemaphore(d, &sci, NULL, &s2));
  VkSubmitInfo sub = {VK_STRUCTURE_TYPE_SUBMIT_INFO, .signalSemaphoreCount = 1, .pSignalSemaphores = &s2};
  printf("submit %d\n", vkQueueSubmit(qu, 1, &sub, f2));
  VkFenceGetFdInfoKHR gf = {VK_STRUCTURE_TYPE_FENCE_GET_FD_INFO_KHR, .fence = f2, .handleType = VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT};
  int fd = -2; printf("get fence fd %d fd=%d\n", vkGetFenceFdKHR(d, &gf, &fd), fd);
  VkSemaphoreGetFdInfoKHR gs = {VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR, .semaphore = s2, .handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT};
  int sfd = -2; printf("get sem fd %d fd=%d\n", vkGetSemaphoreFdKHR(d, &gs, &sfd), sfd);
  if (sfd >= 0) { is.fd = sfd; printf("reimport sem fd %d\n", vkImportSemaphoreFdKHR(d, &is)); }
  printf("queue idle %d\n", vkQueueWaitIdle(qu));
  return 0;
}
