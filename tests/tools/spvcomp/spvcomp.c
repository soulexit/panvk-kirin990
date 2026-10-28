/* spvcomp <icd.so> <shader.vert.spv> [frag.spv] : build a graphics pipeline around
 * a vertex shader captured from a game (MESA_SPIRV_DUMP_PATH), so the driver
 * compiles it as it would in the game. The descriptor set layout and vertex input
 * are derived from the SPIR-V (DXVK style: set 0, UBOs, SSBOs, buffer views,
 * images, samplers); the fragment shader is generated to read every output as a
 * vec4 unless one is given. Run with the host drm-shim build and
 * BIFROST_MESA_DEBUG=shaders,shaderdb to get the Bifrost code and statistics.
 * Robustness2 is enabled like DXVK does. SPVCOMP_REPEAT=n compiles n times
 * (timing). */
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static PFN_vkGetInstanceProcAddr gipa;
static VkInstance inst;
static VkDevice dev;
#define I(n) PFN_##n n = (PFN_##n)gipa(inst, #n)
#define D(n) PFN_##n n = (PFN_##n)gdpa(dev, #n)
#define CK(x) do { VkResult r_ = (x); if (r_) { printf("FAIL %s = %d\n", #x, r_); exit(1); } } while (0)

static uint32_t *load(const char *path, size_t *words)
{
   FILE *f = fopen(path, "rb");
   if (!f) { perror(path); exit(1); }
   fseek(f, 0, SEEK_END);
   long n = ftell(f);
   fseek(f, 0, SEEK_SET);
   uint32_t *w = malloc(n);
   if (fread(w, 1, n, f) != (size_t)n) { perror("read"); exit(1); }
   fclose(f);
   *words = n / 4;
   return w;
}

/* Minimal SPIR-V reflection. */
#define MAXID 65536
static uint32_t op_of[MAXID], arg0[MAXID], arg1[MAXID], arg2[MAXID], arg3[MAXID];
static int set_of[MAXID], binding_of[MAXID], location_of[MAXID], builtin_of[MAXID];
static int block_of[MAXID], bufblock_of[MAXID];

struct binding { int binding; VkDescriptorType type; };

static uint32_t strip_ptr_array(uint32_t t)
{
   for (;;) {
      if (op_of[t] == 32 /* OpTypePointer */) t = arg1[t];
      else if (op_of[t] == 28 || op_of[t] == 29 /* (Runtime)Array */) t = arg0[t];
      else return t;
   }
}

#include <time.h>
static double now_ms(void)
{
   struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
   return t.tv_sec * 1e3 + t.tv_nsec / 1e6;
}

/* Draw `mverts` million vertices with the captured vertex shader. Every buffer
 * is zero (bone index 0, weight 0, matrices 0): positions collapse, so every
 * triangle is culled and the time is position shading plus culling, which is
 * what NieR:Automata's vertex/tiler batches are waiting on. */
static void
bench(VkPhysicalDevice pd, PFN_vkVoidFunction gmp_, PFN_vkGetDeviceProcAddr gdpa, VkRenderPass rp,
      VkPipelineLayout pl, VkPipeline pipe, VkDescriptorSetLayout dsl,
      const struct binding *binds, int nbinds, int mverts)
{
   PFN_vkGetPhysicalDeviceMemoryProperties gmp = (PFN_vkGetPhysicalDeviceMemoryProperties)gmp_;
   D(vkCreateBuffer); D(vkGetBufferMemoryRequirements); D(vkAllocateMemory); D(vkBindBufferMemory);
   D(vkCreateImage); D(vkGetImageMemoryRequirements); D(vkBindImageMemory); D(vkCreateImageView);
   D(vkCreateFramebuffer); D(vkCreateDescriptorPool); D(vkAllocateDescriptorSets);
   D(vkUpdateDescriptorSets); D(vkCreateBufferView); D(vkCreateCommandPool);
   D(vkAllocateCommandBuffers); D(vkBeginCommandBuffer); D(vkEndCommandBuffer);
   D(vkCmdBeginRenderPass); D(vkCmdEndRenderPass); D(vkCmdBindPipeline); D(vkCmdSetViewport);
   D(vkCmdSetScissor); D(vkCmdBindDescriptorSets); D(vkCmdBindVertexBuffers); D(vkCmdDraw);
   D(vkQueueSubmit); D(vkQueueWaitIdle); D(vkGetDeviceQueue); D(vkMapMemory);
   VkPhysicalDeviceMemoryProperties mp; gmp(pd, &mp);

#define ALLOC(req, mem, host) do { \
      uint32_t ti_ = 0; \
      const VkMemoryPropertyFlags want_ = host ? (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | \
                                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) \
                                               : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT; \
      while (!((req).memoryTypeBits & (1u << ti_)) || \
             (mp.memoryTypes[ti_].propertyFlags & want_) != want_) ti_++; \
      VkMemoryAllocateInfo mai_ = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, \
                                   .allocationSize = (req).size, .memoryTypeIndex = ti_}; \
      CK(vkAllocateMemory(dev, &mai_, NULL, &(mem))); } while (0)

   /* One zeroed host-visible buffer for everything. */
   const VkDeviceSize bsize = 8 << 20;
   VkBufferCreateInfo bci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = bsize,
      .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
               VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT |
               VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT};
   VkBuffer buf; CK(vkCreateBuffer(dev, &bci, NULL, &buf));
   VkMemoryRequirements mr; vkGetBufferMemoryRequirements(dev, buf, &mr);
   VkDeviceMemory bmem; ALLOC(mr, bmem, 1);
   CK(vkBindBufferMemory(dev, buf, bmem, 0));
   void *map; CK(vkMapMemory(dev, bmem, 0, VK_WHOLE_SIZE, 0, &map));
   memset(map, 0, bsize);

   VkImageCreateInfo ici = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .imageType = VK_IMAGE_TYPE_2D,
      .format = VK_FORMAT_R8G8B8A8_UNORM, .extent = {256, 256, 1}, .mipLevels = 1,
      .arrayLayers = 1, .samples = VK_SAMPLE_COUNT_1_BIT, .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT};
   VkImage img; CK(vkCreateImage(dev, &ici, NULL, &img));
   VkMemoryRequirements imr; vkGetImageMemoryRequirements(dev, img, &imr);
   VkDeviceMemory imem; ALLOC(imr, imem, 0);
   CK(vkBindImageMemory(dev, img, imem, 0));
   VkImageViewCreateInfo ivi = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = img,
      .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = VK_FORMAT_R8G8B8A8_UNORM,
      .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
   VkImageView iv; CK(vkCreateImageView(dev, &ivi, NULL, &iv));
   VkFramebufferCreateInfo fbi = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, .renderPass = rp,
      .attachmentCount = 1, .pAttachments = &iv, .width = 256, .height = 256, .layers = 1};
   VkFramebuffer fb; CK(vkCreateFramebuffer(dev, &fbi, NULL, &fb));

   VkDescriptorPoolSize ps[6] = {
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 32}, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 32},
      {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 32}, {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 32},
      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 32}, {VK_DESCRIPTOR_TYPE_SAMPLER, 32}};
   VkDescriptorPoolCreateInfo dpi = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .maxSets = 1,
                                     .poolSizeCount = 6, .pPoolSizes = ps};
   VkDescriptorPool dp; CK(vkCreateDescriptorPool(dev, &dpi, NULL, &dp));
   VkDescriptorSetAllocateInfo dsa = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = dp, .descriptorSetCount = 1, .pSetLayouts = &dsl};
   VkDescriptorSet set; CK(vkAllocateDescriptorSets(dev, &dsa, &set));
   for (int i = 0; i < nbinds; i++) {
      VkDescriptorBufferInfo dbi = {buf, 0, binds[i].type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
                                               ? 65536 : (4 << 20)};
      VkBufferView bv = VK_NULL_HANDLE;
      VkWriteDescriptorSet wds = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = set,
         .dstBinding = binds[i].binding, .descriptorCount = 1, .descriptorType = binds[i].type};
      if (binds[i].type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
          binds[i].type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
         wds.pBufferInfo = &dbi;
      } else if (binds[i].type == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
                 binds[i].type == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER) {
         VkBufferViewCreateInfo bvi = {VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO, .buffer = buf,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT, .range = 1 << 20};
         CK(vkCreateBufferView(dev, &bvi, NULL, &bv));
         wds.pTexelBufferView = &bv;
      } else {
         printf("bench: binding %d type %d not supported\n", binds[i].binding, binds[i].type);
         exit(1);
      }
      vkUpdateDescriptorSets(dev, 1, &wds, 0, NULL);
   }

   VkQueue q; vkGetDeviceQueue(dev, 0, 0, &q);
   VkCommandPoolCreateInfo cpi = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
   VkCommandPool cp; CK(vkCreateCommandPool(dev, &cpi, NULL, &cp));
   VkCommandBufferAllocateInfo cbi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                      .commandPool = cp, .commandBufferCount = 1};
   VkCommandBuffer cb; CK(vkAllocateCommandBuffers(dev, &cbi, &cb));
   VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
   CK(vkBeginCommandBuffer(cb, &bi));
   VkClearValue clr = {0};
   VkRenderPassBeginInfo rbi = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, .renderPass = rp,
      .framebuffer = fb, .renderArea = {{0, 0}, {256, 256}}, .clearValueCount = 1,
      .pClearValues = &clr};
   vkCmdBeginRenderPass(cb, &rbi, VK_SUBPASS_CONTENTS_INLINE);
   vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
   VkViewport vpt = {0, 0, 256, 256, 0, 1};
   VkRect2D sc = {{0, 0}, {256, 256}};
   vkCmdSetViewport(cb, 0, 1, &vpt);
   vkCmdSetScissor(cb, 0, 1, &sc);
   vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pl, 0, 1, &set, 0, NULL);
   VkDeviceSize off = 0;
   vkCmdBindVertexBuffers(cb, 0, 1, &buf, &off);
   /* Vertex buffer stride 256 within 8 MiB: 32768 vertices, so repeat a draw of
    * 32766 vertices (vertex reads stay in bounds). */
   const uint32_t per = 32766, draws = (uint32_t)((double)mverts * 1e6 / per);
   for (uint32_t d = 0; d < draws; d++)
      vkCmdDraw(cb, per, 1, 0, 0);
   vkCmdEndRenderPass(cb);
   CK(vkEndCommandBuffer(cb));
   VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1,
                      .pCommandBuffers = &cb};
   double ms[6];
   for (int r = 0; r < 6; r++) {
      const double a = now_ms();
      CK(vkQueueSubmit(q, 1, &si, VK_NULL_HANDLE));
      CK(vkQueueWaitIdle(q));
      ms[r] = now_ms() - a;
   }
   double best = ms[1];
   for (int r = 2; r < 6; r++) if (ms[r] < best) best = ms[r];
   const double nv = (double)draws * per;
   printf("bench: %.2fM vertices in %.2f ms: %.2f ns/vertex (best of 5)\n", nv / 1e6, best,
          best * 1e6 / nv);
}

/* Compute mode: one zeroed 8 MiB buffer behind every binding, dispatch
 * SPVCOMP_GROUPS (default 3) workgroups, wait up to 10 s. */
static int
run_compute(VkPhysicalDevice pd, PFN_vkVoidFunction gmp_, PFN_vkGetDeviceProcAddr gdpa,
            VkPipelineLayout pl, VkDescriptorSetLayout dsl, const struct binding *binds,
            int nbinds, const uint32_t *spv, size_t nw)
{
   PFN_vkGetPhysicalDeviceMemoryProperties gmp = (PFN_vkGetPhysicalDeviceMemoryProperties)gmp_;
   D(vkCreateShaderModule); D(vkCreateComputePipelines); D(vkCreateBuffer);
   D(vkGetBufferMemoryRequirements); D(vkAllocateMemory); D(vkBindBufferMemory); D(vkMapMemory);
   D(vkCreateDescriptorPool); D(vkAllocateDescriptorSets); D(vkUpdateDescriptorSets);
   D(vkCreateBufferView); D(vkCreateCommandPool); D(vkAllocateCommandBuffers);
   D(vkBeginCommandBuffer); D(vkEndCommandBuffer); D(vkCmdBindPipeline);
   D(vkCmdBindDescriptorSets); D(vkCmdDispatch); D(vkQueueSubmit); D(vkGetDeviceQueue);
   D(vkCreateFence); D(vkWaitForFences);
   VkShaderModuleCreateInfo smi = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = nw * 4,
                                   .pCode = spv};
   VkShaderModule sm; CK(vkCreateShaderModule(dev, &smi, NULL, &sm));
   VkComputePipelineCreateInfo cpi = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = sm, .pName = "main"}, .layout = pl};
   VkPipeline pipe;
   double t0 = now_ms();
   CK(vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &cpi, NULL, &pipe));
   printf("compute pipeline compiled in %.1f ms\n", now_ms() - t0);

   VkPhysicalDeviceMemoryProperties mp; gmp(pd, &mp);
   const VkDeviceSize bsize = 8 << 20;
   VkBufferCreateInfo bci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = bsize,
      .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
               VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT};
   VkBuffer buf; CK(vkCreateBuffer(dev, &bci, NULL, &buf));
   VkMemoryRequirements mr; vkGetBufferMemoryRequirements(dev, buf, &mr);
   uint32_t ti = 0;
   const VkMemoryPropertyFlags want = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
   while (!(mr.memoryTypeBits & (1u << ti)) || (mp.memoryTypes[ti].propertyFlags & want) != want) ti++;
   VkMemoryAllocateInfo mai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = mr.size,
                               .memoryTypeIndex = ti};
   VkDeviceMemory mem; CK(vkAllocateMemory(dev, &mai, NULL, &mem));
   CK(vkBindBufferMemory(dev, buf, mem, 0));
   void *map; CK(vkMapMemory(dev, mem, 0, VK_WHOLE_SIZE, 0, &map));
   memset(map, 0, bsize);
   if (getenv("SPVCOMP_FILL")) {
      /* hex word, or "rand" for pseudo-random words */
      const char *fv = getenv("SPVCOMP_FILL");
      uint32_t x = 12345, v = (uint32_t)strtoul(fv, NULL, 16);
      for (size_t i = 0; i < bsize / 4; i++) {
         x = x * 1103515245u + 12345u;
         ((uint32_t *)map)[i] = strcmp(fv, "rand") ? v : x;
      }
   }
   VkDescriptorPoolSize ps[4] = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 64},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 64}, {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 64},
      {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 64}};
   VkDescriptorPoolCreateInfo dpi = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .maxSets = 1,
                                     .poolSizeCount = 4, .pPoolSizes = ps};
   VkDescriptorPool dp; CK(vkCreateDescriptorPool(dev, &dpi, NULL, &dp));
   VkDescriptorSetAllocateInfo dsa = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = dp, .descriptorSetCount = 1, .pSetLayouts = &dsl};
   VkDescriptorSet set; CK(vkAllocateDescriptorSets(dev, &dsa, &set));
   for (int i = 0; i < nbinds; i++) {
      VkDescriptorBufferInfo dbi = {buf, 0, binds[i].type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ? 65536 : (4 << 20)};
      VkBufferView bv = VK_NULL_HANDLE;
      VkWriteDescriptorSet wds = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = set,
         .dstBinding = binds[i].binding, .descriptorCount = 1, .descriptorType = binds[i].type};
      if (binds[i].type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
          binds[i].type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
         wds.pBufferInfo = &dbi;
      } else if (binds[i].type == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
                 binds[i].type == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER) {
         VkBufferViewCreateInfo bvi = {VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO, .buffer = buf,
            .format = VK_FORMAT_R32_UINT, .range = 1 << 20};
         CK(vkCreateBufferView(dev, &bvi, NULL, &bv));
         wds.pTexelBufferView = &bv;
      } else {
         printf("compute: binding %d type %d not supported\n", binds[i].binding, binds[i].type);
         return 1;
      }
      vkUpdateDescriptorSets(dev, 1, &wds, 0, NULL);
   }
   VkQueue q; vkGetDeviceQueue(dev, 0, 0, &q);
   VkCommandPoolCreateInfo cpci = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
   VkCommandPool cp; CK(vkCreateCommandPool(dev, &cpci, NULL, &cp));
   VkCommandBufferAllocateInfo cbi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                      .commandPool = cp, .commandBufferCount = 1};
   VkCommandBuffer cb; CK(vkAllocateCommandBuffers(dev, &cbi, &cb));
   VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
   CK(vkBeginCommandBuffer(cb, &bi));
   vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipe);
   vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pl, 0, 1, &set, 0, NULL);
   const uint32_t groups = getenv("SPVCOMP_GROUPS") ? atoi(getenv("SPVCOMP_GROUPS")) : 3;
   vkCmdDispatch(cb, groups, 1, 1);
   CK(vkEndCommandBuffer(cb));
   VkFenceCreateInfo fci = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
   VkFence fence; CK(vkCreateFence(dev, &fci, NULL, &fence));
   VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cb};
   t0 = now_ms();
   CK(vkQueueSubmit(q, 1, &si, fence));
   VkResult r = vkWaitForFences(dev, 1, &fence, VK_TRUE, 10000000000ull);
   printf("dispatch %u groups: %s after %.1f ms\n", groups,
          r == VK_SUCCESS ? "done" : r == VK_TIMEOUT ? "TIMEOUT" : "ERROR", now_ms() - t0);
   if (r) printf("FAIL wait %d\n", r);
   else printf("PASS\n");
   return r ? 1 : 0;
}

int main(int argc, char **argv)
{
   if (argc < 3) { printf("usage: spvcomp icd.so shader.vert.spv [frag.spv]\n"); return 2; }
   size_t nw;
   uint32_t *w = load(argv[2], &nw);
   for (int i = 0; i < MAXID; i++) set_of[i] = binding_of[i] = location_of[i] = builtin_of[i] = -1;

   /* Pass 1: decorations, types, variables. */
   struct { uint32_t id, type, sc; } vars[512]; int nvars = 0;
   int is_cs = 0;
   for (size_t p = 5; p < nw;) {
      const uint32_t op = w[p] & 0xffff, len = w[p] >> 16;
      const uint32_t *a = &w[p + 1];
      if (!len) break;
      if (op == 15 /* OpEntryPoint */ && a[0] == 5 /* GLCompute */)
         is_cs = 1;
      if (op == 71 /* OpDecorate */) {
         if (a[1] == 34) set_of[a[0]] = a[2];
         else if (a[1] == 33) binding_of[a[0]] = a[2];
         else if (a[1] == 30) location_of[a[0]] = a[2];
         else if (a[1] == 11) builtin_of[a[0]] = a[2];
         else if (a[1] == 2) block_of[a[0]] = 1;
         else if (a[1] == 3) bufblock_of[a[0]] = 1;
      } else if (op >= 19 && op <= 39 && len >= 2) { /* types: result id first */
         op_of[a[0]] = op;
         arg0[a[0]] = len > 2 ? a[1] : 0;
         arg1[a[0]] = len > 3 ? a[2] : 0;
         arg2[a[0]] = len > 4 ? a[3] : 0;
         arg3[a[0]] = len > 5 ? a[4] : 0;
      } else if (op == 59 /* OpVariable */ && nvars < 512) {
         vars[nvars].type = a[0];
         vars[nvars].id = a[1];
         vars[nvars].sc = a[2];
         nvars++;
      }
      p += len;
   }

   struct binding binds[64]; int nbinds = 0;
   VkVertexInputAttributeDescription attrs[16]; int nattrs = 0;
   char frag_src[8192] = "#version 450\nlayout(location = 0) out vec4 color;\n";
   char frag_body[4096] = "void main() {\n   color = vec4(0.0);\n";
   for (int v = 0; v < nvars; v++) {
      const uint32_t id = vars[v].id, sc = vars[v].sc;
      const uint32_t ptr = vars[v].type;
      uint32_t t = strip_ptr_array(ptr);
      if (binding_of[id] >= 0) {
         VkDescriptorType dt;
         if (sc == 2 /* Uniform */)
            dt = bufblock_of[t] ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
                                : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
         else if (sc == 12 /* StorageBuffer */)
            dt = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
         else if (op_of[t] == 26 /* OpTypeSampler */)
            dt = VK_DESCRIPTOR_TYPE_SAMPLER;
         else if (op_of[t] == 27 /* OpTypeSampledImage */)
            dt = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
         else if (op_of[t] == 25 /* OpTypeImage: sampled type, dim, depth, arrayed, ms, sampled */) {
            /* arg1 = dim, sampled is the 7th word: re-read from the stream. */
            uint32_t dim = arg1[t], sampled = 0;
            for (size_t p = 5; p < nw;) {
               const uint32_t op = w[p] & 0xffff, len = w[p] >> 16;
               if (op == 25 && w[p + 1] == t) { sampled = w[p + 7]; break; }
               p += len;
            }
            if (dim == 5 /* Buffer */)
               dt = sampled == 2 ? VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER
                                 : VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
            else
               dt = sampled == 2 ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
                                 : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
         } else {
            printf("unknown resource type for binding %d (sc %u op %u)\n", binding_of[id],
                   sc, op_of[t]);
            return 1;
         }
         binds[nbinds].binding = binding_of[id];
         binds[nbinds].type = dt;
         nbinds++;
      } else if (sc == 1 /* Input */ && location_of[id] >= 0 && nattrs < 16) {
         uint32_t comp = t, n = 1;
         if (op_of[t] == 23 /* OpTypeVector */) { comp = arg0[t]; n = arg1[t]; }
         const int is_float = op_of[comp] == 22;
         const int is_signed = op_of[comp] == 21 && arg1[comp];
         static const VkFormat ff[] = {0, VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT,
            VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT};
         static const VkFormat fu[] = {0, VK_FORMAT_R32_UINT, VK_FORMAT_R32G32_UINT,
            VK_FORMAT_R32G32B32_UINT, VK_FORMAT_R32G32B32A32_UINT};
         static const VkFormat fs[] = {0, VK_FORMAT_R32_SINT, VK_FORMAT_R32G32_SINT,
            VK_FORMAT_R32G32B32_SINT, VK_FORMAT_R32G32B32A32_SINT};
         attrs[nattrs] = (VkVertexInputAttributeDescription){
            .location = location_of[id], .binding = 0,
            .format = is_float ? ff[n] : is_signed ? fs[n] : fu[n],
            .offset = 16 * location_of[id]};
         nattrs++;
      } else if (sc == 3 /* Output */ && location_of[id] >= 0) {
         uint32_t comp = t, n = 1;
         if (op_of[t] == 23) { comp = arg0[t]; n = arg1[t]; }
         const int is_float = op_of[comp] == 22;
         static const char *fv[] = {"", "float", "vec2", "vec3", "vec4"};
         static const char *iv[] = {"", "int", "ivec2", "ivec3", "ivec4"};
         static const char *uv[] = {"", "uint", "uvec2", "uvec3", "uvec4"};
         const int is_signed = op_of[comp] == 21 && arg1[comp];
         char line[160];
         snprintf(line, sizeof(line), "layout(location = %d) %sin %s i%d;\n", location_of[id],
                  is_float ? "" : "flat ", is_float ? fv[n] : is_signed ? iv[n] : uv[n],
                  location_of[id]);
         strcat(frag_src, line);
         snprintf(line, sizeof(line), "   color += vec4(%s(i%d)%s);\n",
                  is_float ? "" : "", location_of[id],
                  n == 4 ? "" : n == 3 ? ", 0.0" : n == 2 ? ", 0.0, 0.0" : ", 0.0, 0.0, 0.0");
         if (!is_float) {
            static const char *cv[] = {"", "float", "vec2", "vec3", "vec4"};
            snprintf(line, sizeof(line), "   color += vec4(%s(i%d)%s);\n", cv[n], location_of[id],
                     n == 4 ? "" : n == 3 ? ", 0.0" : n == 2 ? ", 0.0, 0.0" : ", 0.0, 0.0, 0.0");
         }
         strcat(frag_body, line);
      }
   }
   strcat(frag_body, "}\n");
   strcat(frag_src, frag_body);

   size_t fw = 0;
   uint32_t *fspv = NULL;
   if (is_cs) {
   } else if (argc > 3) {
      fspv = load(argv[3], &fw);
   } else {
      FILE *f = fopen("/tmp/spvcomp_frag.frag", "w");
      fputs(frag_src, f);
      fclose(f);
      if (system("glslangValidator -V -S frag /tmp/spvcomp_frag.frag -o /tmp/spvcomp_frag.spv >/dev/null")) {
         printf("glslang failed:\n%s", frag_src);
         return 1;
      }
      fspv = load("/tmp/spvcomp_frag.spv", &fw);
   }
   fprintf(stderr, "spvcomp: %d bindings, %d vertex inputs\n", nbinds, nattrs);

   void *h = dlopen(argv[1], RTLD_NOW);
   if (!h) { printf("dlopen %s\n", dlerror()); return 1; }
   gipa = (PFN_vkGetInstanceProcAddr)dlsym(h, "vk_icdGetInstanceProcAddr");
   I(vkCreateInstance);
   VkApplicationInfo ai = {VK_STRUCTURE_TYPE_APPLICATION_INFO, .apiVersion = VK_API_VERSION_1_3};
   VkInstanceCreateInfo ici = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &ai};
   CK(vkCreateInstance(&ici, NULL, &inst));
   I(vkEnumeratePhysicalDevices); I(vkCreateDevice); I(vkGetDeviceProcAddr);
   I(vkGetPhysicalDeviceFeatures2);
   PFN_vkGetDeviceProcAddr gdpa = vkGetDeviceProcAddr;
   uint32_t n = 1; VkPhysicalDevice pd = NULL; vkEnumeratePhysicalDevices(inst, &n, &pd);
   if (!n || !pd) { printf("FAIL no physical device\n"); return 1; }

   /* Every supported core feature, plus robustness2 as DXVK enables it. */
   VkPhysicalDeviceRobustness2FeaturesEXT r2 = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT};
   VkPhysicalDeviceVulkan13Features f13 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES, &r2};
   VkPhysicalDeviceVulkan12Features f12 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, &f13};
   VkPhysicalDeviceVulkan11Features f11 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES, &f12};
   const int plain = getenv("SPVCOMP_PLAIN") != NULL; /* Vulkan 1.1 driver (vendor) */
   VkPhysicalDeviceFeatures2 f2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                                   plain ? NULL : (void *)&f11};
   vkGetPhysicalDeviceFeatures2(pd, &f2);
   VkPhysicalDeviceShaderDrawParametersFeatures dp = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES, .shaderDrawParameters = 1};
   if (plain)
      f2.pNext = &dp;
   r2.robustImageAccess2 = r2.robustImageAccess2 && !getenv("SPVCOMP_NOROBUST");
   r2.robustBufferAccess2 = r2.robustBufferAccess2 && !getenv("SPVCOMP_NOROBUST");
   f2.features.robustBufferAccess = !getenv("SPVCOMP_NOROBUST");
   fprintf(stderr, "spvcomp: robustBufferAccess2 %u robustImageAccess2 %u nullDescriptor %u\n",
           r2.robustBufferAccess2, r2.robustImageAccess2, r2.nullDescriptor);
   const char *exts[] = {VK_EXT_ROBUSTNESS_2_EXTENSION_NAME};
   float prio = 1;
   VkDeviceQueueCreateInfo qci = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueCount = 1,
                                  .pQueuePriorities = &prio};
   VkDeviceCreateInfo dci = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, &f2, .queueCreateInfoCount = 1,
                             .pQueueCreateInfos = &qci, .enabledExtensionCount = plain ? 0 : 1,
                             .ppEnabledExtensionNames = exts};
   CK(vkCreateDevice(pd, &dci, NULL, &dev));
   D(vkCreateShaderModule); D(vkCreateDescriptorSetLayout); D(vkCreatePipelineLayout);
   D(vkCreateGraphicsPipelines); D(vkDestroyPipeline); D(vkCreateRenderPass);

   VkDescriptorSetLayoutBinding lb[64];
   for (int i = 0; i < nbinds; i++)
      lb[i] = (VkDescriptorSetLayoutBinding){binds[i].binding, binds[i].type, 1,
                                             is_cs ? VK_SHADER_STAGE_COMPUTE_BIT
                                                   : VK_SHADER_STAGE_ALL_GRAPHICS};
   VkDescriptorSetLayoutCreateInfo dli = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                          .bindingCount = nbinds, .pBindings = lb};
   VkDescriptorSetLayout dsl; CK(vkCreateDescriptorSetLayout(dev, &dli, NULL, &dsl));
   VkPushConstantRange pcr = {is_cs ? VK_SHADER_STAGE_COMPUTE_BIT : VK_SHADER_STAGE_ALL_GRAPHICS,
                              0, 128};
   VkPipelineLayoutCreateInfo pli = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                     .setLayoutCount = 1, .pSetLayouts = &dsl,
                                     .pushConstantRangeCount = 1, .pPushConstantRanges = &pcr};
   VkPipelineLayout pl; CK(vkCreatePipelineLayout(dev, &pli, NULL, &pl));
   if (is_cs)
      return run_compute(pd, gipa(inst, "vkGetPhysicalDeviceMemoryProperties"), gdpa, pl, dsl,
                         binds, nbinds, w, nw);

   VkShaderModuleCreateInfo vmi = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                   .codeSize = nw * 4, .pCode = w};
   VkShaderModule vsm; CK(vkCreateShaderModule(dev, &vmi, NULL, &vsm));
   VkShaderModuleCreateInfo fmi = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                   .codeSize = fw * 4, .pCode = fspv};
   VkShaderModule fsm; CK(vkCreateShaderModule(dev, &fmi, NULL, &fsm));

   VkPipelineShaderStageCreateInfo st[2] = {
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT,
       .module = vsm, .pName = "main"},
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
       .module = fsm, .pName = "main"}};
   VkVertexInputBindingDescription vb = {0, 256, VK_VERTEX_INPUT_RATE_VERTEX};
   VkPipelineVertexInputStateCreateInfo vi = {
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = nattrs ? 1 : 0, .pVertexBindingDescriptions = &vb,
      .vertexAttributeDescriptionCount = nattrs, .pVertexAttributeDescriptions = attrs};
   VkPipelineInputAssemblyStateCreateInfo ia = {
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
   VkPipelineViewportStateCreateInfo vp = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                                           .viewportCount = 1, .scissorCount = 1};
   VkPipelineRasterizationStateCreateInfo rs = {
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .polygonMode = VK_POLYGON_MODE_FILL, .cullMode = VK_CULL_MODE_BACK_BIT, .lineWidth = 1};
   VkPipelineMultisampleStateCreateInfo ms = {
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
   VkPipelineColorBlendAttachmentState cba = {.colorWriteMask = 0xf};
   VkPipelineColorBlendStateCreateInfo cb = {
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, .attachmentCount = 1,
      .pAttachments = &cba};
   VkDynamicState dyn[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
   VkPipelineDynamicStateCreateInfo dy = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                                          .dynamicStateCount = 2, .pDynamicStates = dyn};
   VkAttachmentDescription att = {.format = VK_FORMAT_R8G8B8A8_UNORM,
      .samples = VK_SAMPLE_COUNT_1_BIT, .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE, .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
   VkAttachmentReference ar = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
   VkSubpassDescription sp = {.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                              .colorAttachmentCount = 1, .pColorAttachments = &ar};
   VkRenderPassCreateInfo rpi = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                                 .attachmentCount = 1, .pAttachments = &att,
                                 .subpassCount = 1, .pSubpasses = &sp};
   VkRenderPass rp; CK(vkCreateRenderPass(dev, &rpi, NULL, &rp));
   VkGraphicsPipelineCreateInfo gpi = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .stageCount = 2, .pStages = st, .pVertexInputState = &vi, .pInputAssemblyState = &ia,
      .pViewportState = &vp, .pRasterizationState = &rs, .pMultisampleState = &ms,
      .pColorBlendState = &cb, .pDynamicState = &dy, .layout = pl, .renderPass = rp};
   const int repeat = getenv("SPVCOMP_REPEAT") ? atoi(getenv("SPVCOMP_REPEAT")) : 1;
   for (int i = 0; i < repeat; i++) {
      VkPipeline pipe;
      CK(vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &gpi, NULL, &pipe));
      vkDestroyPipeline(dev, pipe, NULL);
   }
   if (getenv("SPVCOMP_BENCH")) {
      VkPipeline pipe;
      CK(vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &gpi, NULL, &pipe));
      bench(pd, gipa(inst, "vkGetPhysicalDeviceMemoryProperties"), gdpa, rp, pl, pipe, dsl, binds,
            nbinds, atoi(getenv("SPVCOMP_BENCH")));
   }
   printf("PASS\n");
   return 0;
}
