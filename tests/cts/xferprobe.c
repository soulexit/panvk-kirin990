/* xferprobe <icd.so> : vkCmdUpdateBuffer -> CopyBufferToImage -> CopyImageToBuffer, like
 * dEQP-VK.api.smoke.transfer.  Env: EXTS=all|none|list, FEATS=all|none, NOEXT=a,b (skip these),
 * NOFEAT=<struct-sType-number>,... (skip these feature structs), ITER=n. */
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>

#define N 32
static PFN_vkGetInstanceProcAddr gipa;
static VkInstance inst;
#define L(n) PFN_##n n = (PFN_##n)gipa(inst, #n)

static int listed(const char *list, const char *name)
{
   if (!list) return 0;
   size_t l = strlen(name);
   for (const char *p = strstr(list, name); p; p = strstr(p + 1, name))
      if ((p == list || p[-1] == ',') && (p[l] == ',' || !p[l])) return 1;
   return 0;
}

int main(int argc, char **argv)
{
   void *h = dlopen(argc > 1 ? argv[1] : "./libvulkan_panfrost.so", RTLD_NOW);
   if (!h) { printf("dlopen: %s\n", dlerror()); return 1; }
   gipa = (PFN_vkGetInstanceProcAddr)dlsym(h, "vk_icdGetInstanceProcAddr");
   L(vkCreateInstance);
   VkApplicationInfo ai = {VK_STRUCTURE_TYPE_APPLICATION_INFO, .apiVersion = VK_API_VERSION_1_3};
   VkInstanceCreateInfo ici = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &ai};
   if (vkCreateInstance(&ici, NULL, &inst)) return 1;
   L(vkEnumeratePhysicalDevices); L(vkGetPhysicalDeviceFeatures2); L(vkCreateDevice);
   L(vkEnumerateDeviceExtensionProperties); L(vkGetDeviceProcAddr);
   L(vkGetPhysicalDeviceMemoryProperties);
   uint32_t n = 1; VkPhysicalDevice pd; vkEnumeratePhysicalDevices(inst, &n, &pd);

   const char *exts_env = getenv("EXTS"), *noext = getenv("NOEXT"), *feats_env = getenv("FEATS");
   const char *nofeat = getenv("NOFEAT");
   uint32_t ne = 0; vkEnumerateDeviceExtensionProperties(pd, NULL, &ne, NULL);
   VkExtensionProperties *ep = calloc(ne, sizeof(*ep));
   vkEnumerateDeviceExtensionProperties(pd, NULL, &ne, ep);
   const char **en = calloc(ne, sizeof(char *)); uint32_t nen = 0;
   for (uint32_t i = 0; i < ne; i++) {
      const char *nm = ep[i].extensionName;
      int want = exts_env && (!strcmp(exts_env, "all") || listed(exts_env, nm));
      if (want && !listed(noext, nm)) en[nen++] = nm;
   }

   /* Feature chain: every core/extension feature struct the driver fills, all enabled. */
   VkPhysicalDeviceFeatures2 f2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
   VkPhysicalDeviceVulkan11Features v11 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
   VkPhysicalDeviceVulkan12Features v12 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
   VkPhysicalDeviceVulkan13Features v13 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
   VkPhysicalDeviceRobustness2FeaturesEXT rb2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT};
   void *chain[] = {&v11, &v12, &v13, &rb2};
   void **tail = &f2.pNext;
   for (unsigned i = 0; i < sizeof(chain) / sizeof(chain[0]); i++) {
      char key[16]; snprintf(key, sizeof key, "%u", i);
      if (listed(nofeat, key)) continue;
      if (i == 3 && !listed(getenv("EXTS") ? getenv("EXTS") : "", "VK_EXT_robustness2") &&
          !(exts_env && !strcmp(exts_env, "all")))
         continue;
      *tail = chain[i]; tail = &((VkBaseOutStructure *)chain[i])->pNext;
   }
   vkGetPhysicalDeviceFeatures2(pd, &f2);
   if (!feats_env || strcmp(feats_env, "all")) {
      VkPhysicalDeviceFeatures2 z = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
      f2 = z;
   }
   if (listed(nofeat, "rba")) f2.features.robustBufferAccess = 0;
   float prio = 1;
   VkDeviceQueueCreateInfo qci = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueCount = 1,
                                  .pQueuePriorities = &prio};
   VkDeviceCreateInfo dci = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .pNext = &f2,
                             .queueCreateInfoCount = 1, .pQueueCreateInfos = &qci,
                             .enabledExtensionCount = nen, .ppEnabledExtensionNames = en};
   VkDevice dev;
   VkResult r = vkCreateDevice(pd, &dci, NULL, &dev);
   printf("device: %d (%u exts, rba=%u)\n", r, nen, f2.features.robustBufferAccess);
   if (r) return 1;
#define D(n) PFN_##n n = (PFN_##n)vkGetDeviceProcAddr(dev, #n)
   D(vkGetDeviceQueue); D(vkCreateBuffer); D(vkCreateImage); D(vkGetBufferMemoryRequirements);
   D(vkGetImageMemoryRequirements); D(vkAllocateMemory); D(vkBindBufferMemory); D(vkBindImageMemory);
   D(vkMapMemory); D(vkCreateCommandPool); D(vkAllocateCommandBuffers); D(vkBeginCommandBuffer);
   D(vkEndCommandBuffer); D(vkCmdUpdateBuffer); D(vkCmdPipelineBarrier); D(vkCmdCopyBufferToImage);
   D(vkCmdCopyImageToBuffer); D(vkQueueSubmit); D(vkQueueWaitIdle); D(vkInvalidateMappedMemoryRanges);
   D(vkResetCommandPool);
   VkQueue q; vkGetDeviceQueue(dev, 0, 0, &q);
   VkPhysicalDeviceMemoryProperties mp; vkGetPhysicalDeviceMemoryProperties(pd, &mp);

   VkDeviceSize size = N * N * 4;
   VkBufferCreateInfo bci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = size,
      .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT};
   VkBuffer work, res; vkCreateBuffer(dev, &bci, NULL, &work); vkCreateBuffer(dev, &bci, NULL, &res);
   VkImageCreateInfo imci = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .imageType = VK_IMAGE_TYPE_2D,
      .format = VK_FORMAT_R8G8B8A8_UNORM, .extent = {N, N, 1}, .mipLevels = 1, .arrayLayers = 1,
      .samples = 1, .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT};
   VkImage img; vkCreateImage(dev, &imci, NULL, &img);
   /* one allocation, suballocated like the CTS default allocator */
   VkMemoryRequirements rw, rr, ri;
   vkGetBufferMemoryRequirements(dev, work, &rw); vkGetBufferMemoryRequirements(dev, res, &rr);
   vkGetImageMemoryRequirements(dev, img, &ri);
   uint32_t ti = 0;
   while (!(mp.memoryTypes[ti].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) ti++;
   VkDeviceSize off_r = (rw.size + 4095) & ~4095ull, off_i = (off_r + rr.size + 65535) & ~65535ull;
   VkMemoryAllocateInfo mai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = off_i + ri.size,
                               .memoryTypeIndex = ti};
   VkDeviceMemory mem; vkAllocateMemory(dev, &mai, NULL, &mem);
   vkBindBufferMemory(dev, res, mem, off_r);
   if (getenv("DEVLOCAL")) {
      /* work buffer and image in their own DEVICE_LOCAL-only allocations, like CTS "Any" */
      VkMemoryAllocateInfo m0 = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = rw.size,
                                 .memoryTypeIndex = 0};
      VkDeviceMemory mw, mi; vkAllocateMemory(dev, &m0, NULL, &mw);
      m0.allocationSize = ri.size; vkAllocateMemory(dev, &m0, NULL, &mi);
      vkBindBufferMemory(dev, work, mw, 0); vkBindImageMemory(dev, img, mi, 0);
   } else {
      vkBindBufferMemory(dev, work, mem, 0); vkBindImageMemory(dev, img, mem, off_i);
   }
   uint8_t *map; vkMapMemory(dev, mem, 0, VK_WHOLE_SIZE, 0, (void **)&map);
   VkCommandPoolCreateInfo cpci = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
   VkCommandPool pool; vkCreateCommandPool(dev, &cpci, NULL, &pool);
   VkCommandBufferAllocateInfo cbai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = pool,
                                       .commandBufferCount = 1};
   VkCommandBuffer cb; vkAllocateCommandBuffers(dev, &cbai, &cb);

   uint32_t data[N * N];
   for (int i = 0; i < N * N; i++) data[i] = i + 1;
   int iters = getenv("ITER") ? atoi(getenv("ITER")) : 5, bad = 0;
   for (int it = 0; it < iters; it++) {
      memset(map + off_r, 0xff, size);
      vkResetCommandPool(dev, pool, 0);
      VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
      vkBeginCommandBuffer(cb, &bi);
      vkCmdUpdateBuffer(cb, work, 0, size, data);
      VkBufferMemoryBarrier bb = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
         .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT, .srcQueueFamilyIndex = ~0u, .dstQueueFamilyIndex = ~0u,
         .buffer = work, .size = size};
      VkImageMemoryBarrier ib = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
         .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED, .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         .srcQueueFamilyIndex = ~0u, .dstQueueFamilyIndex = ~0u, .image = img,
         .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
      vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 1, &bb, 1, &ib);
      VkBufferImageCopy reg = {.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, .imageExtent = {N, N, 1}};
      vkCmdCopyBufferToImage(cb, work, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &reg);
      ib.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; ib.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      ib.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; ib.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
      vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &ib);
      vkCmdCopyImageToBuffer(cb, img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, res, 1, &reg);
      bb.buffer = res; bb.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
      vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0, NULL, 1, &bb, 0, NULL);
      vkEndCommandBuffer(cb);
      VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cb};
      vkQueueSubmit(q, 1, &si, VK_NULL_HANDLE); vkQueueWaitIdle(q);
      VkMappedMemoryRange mr = {VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, .memory = mem, .size = VK_WHOLE_SIZE};
      vkInvalidateMappedMemoryRanges(dev, 1, &mr);
      uint32_t *out = (uint32_t *)(map + off_r);
      int first = -1, cnt = 0;
      for (int i = 0; i < N * N; i++)
         if (out[i] != data[i]) { if (first < 0) first = i; cnt++; }
      if (cnt) { bad++; printf("iter %d: %d wrong, first at %d = 0x%x\n", it, cnt, first, out[first]); }
   }
   printf("%s (%d/%d bad)\n", bad ? "FAIL" : "PASS", bad, iters);
   return bad != 0;
}
