/* Vertex-pulling draw cost, as DXVK 2.7 draws: argv: icd ndraws indices_per_draw
 * vertex_window frames [pull]. Same draws as idraw.c (indexed, small triangles in
 * per-draw windows of one buffer); pull=1 (default) reads the vertices from a
 * uniform texel buffer by gl_VertexIndex with no vertex buffers bound, pull=0 uses
 * a vertex buffer. Prints ms per frame and per draw. RT 1280x720. */
#define RT_W 1280
#define RT_H 720
#include "dx7_harness.h"
#include "spv.h"
#include "vpull.vert_spv.h"
static uint32_t ndraws, per, pull;
static VkBuffer ib, indb;
static int indirect, noindex, passes;
static VkBuffer dindb;
static VkDescriptorSet set;
static void rec(struct dx7 *t, VkCommandBuffer cmd, void *data) {
   if (pull)
      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, t->layout, 0, 1,
                              &set, 0, NULL);
   vkCmdBindIndexBuffer(cmd, ib, 0, VK_INDEX_TYPE_UINT32);
   for (uint32_t i = 0; i < ndraws; i++) {
      if (passes && i) {
         /* VPULL_PASSES=1: every draw in a render pass of its own. */
         vkCmdEndRenderPass(cmd);
         VkClearValue clear = {.color = {{0.0f, 0.0f, 1.0f, 1.0f}}};
         VkRenderPassBeginInfo rpbi = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = t->rp, .framebuffer = t->fb,
            .renderArea = {{0, 0}, {RT_W, RT_H}},
            .clearValueCount = 1, .pClearValues = &clear};
         vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
      }
      if (noindex)
         vkCmdDrawIndirect(cmd, dindb, i * sizeof(VkDrawIndirectCommand), 1,
                           sizeof(VkDrawIndirectCommand));
      else if (indirect)
         vkCmdDrawIndexedIndirect(cmd, indb, i * sizeof(VkDrawIndexedIndirectCommand), 1,
                                  sizeof(VkDrawIndexedIndirectCommand));
      else
         vkCmdDrawIndexed(cmd, per, 1, i * per, 0, 0);
   }
}
int main(int argc, char **argv) {
   if (argc < 6) { printf("usage: icd ndraws indices_per_draw vertex_window frames [pull]\n"); return 2; }
   ndraws = atoi(argv[2]); per = atoi(argv[3]) / 3 * 3;
   const uint32_t window = atoi(argv[4]); const int frames = atoi(argv[5]);
   pull = argc > 6 ? atoi(argv[6]) : 1;
   /* VPULL_INDIRECT=1: each draw is a vkCmdDrawIndexedIndirect of one command
    * (what DXVK 2.7 records); VPULL_IB_PAD=n adds n unused indices to the index
    * buffer (DXVK shares big index buffers). */
   indirect = getenv("VPULL_INDIRECT") != NULL;
   /* VPULL_DRAW_INDIRECT=1: non-indexed vkCmdDrawIndirect, each draw the
    * first indices_per_draw vertices of its window. */
   noindex = getenv("VPULL_DRAW_INDIRECT") != NULL;
   passes = getenv("VPULL_PASSES") != NULL;
   const uint32_t ib_pad = getenv("VPULL_IB_PAD") ? atoi(getenv("VPULL_IB_PAD")) : 0;
   struct dx7 t; VkPhysicalDeviceFeatures want = {0};
   dx7_init(&t, argv[1], &want);

   const uint32_t nverts = ndraws * window;
   float *v = malloc((size_t)nverts * 16); srand(1);
   for (uint32_t i = 0; i < nverts; i++) {
      float *q = v + (size_t)i * 4;
      q[0] = rand() / (float)RAND_MAX * 2 - 1; q[1] = rand() / (float)RAND_MAX * 2 - 1;
      q[2] = 0.5f; q[3] = (float)i;
   }
   for (uint32_t i = 0; i + 2 < nverts; i += 3)
      for (int k = 1; k < 3; k++) {
         v[(i + k) * 4 + 0] = v[i * 4 + 0] + 0.004f * k;
         v[(i + k) * 4 + 1] = v[i * 4 + 1] + 0.004f * (k == 2);
      }
   VkBuffer vb; VkDeviceMemory vm;
   dx7_buffer(&t, (VkDeviceSize)nverts * 16,
              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT,
              v, &vb, &vm);

   uint32_t *idx = calloc((size_t)ndraws * per + ib_pad, 4);
   const uint32_t tris_in_window = window / 3 ? window / 3 : 1;
   for (uint32_t d = 0; d < ndraws; d++)
      for (uint32_t i = 0; i < per; i += 3) {
         const uint32_t base = d * window + (uint32_t)(rand() % tris_in_window) * 3;
         for (int k = 0; k < 3; k++) idx[(size_t)d * per + i + k] = base + k;
      }
   VkDeviceMemory im;
   dx7_buffer(&t, ((VkDeviceSize)ndraws * per + ib_pad) * 4, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
              idx, &ib, &im);
   VkDrawIndexedIndirectCommand *cmds = calloc(ndraws, sizeof(*cmds));
   for (uint32_t i = 0; i < ndraws; i++)
      cmds[i] = (VkDrawIndexedIndirectCommand){per, 1, i * per, 0, 0};
   VkDrawIndirectCommand *dcmds = calloc(ndraws, sizeof(*dcmds));
   for (uint32_t i = 0; i < ndraws; i++)
      dcmds[i] = (VkDrawIndirectCommand){per, 1, i * window, 0};
   VkDeviceMemory dindm;
   dx7_buffer(&t, ndraws * sizeof(*dcmds), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, dcmds,
              &dindb, &dindm);
   VkDeviceMemory indm;
   dx7_buffer(&t, ndraws * sizeof(*cmds), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, cmds, &indb,
              &indm);

   if (pull) {
      VkDescriptorSetLayoutBinding b = {0, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1,
                                        VK_SHADER_STAGE_VERTEX_BIT, NULL};
      VkDescriptorSetLayoutCreateInfo dl = {
         .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
         .bindingCount = 1, .pBindings = &b};
      VkDescriptorSetLayout sl;
      CK(vkCreateDescriptorSetLayout(t.dev, &dl, NULL, &sl), "SetLayout");
      VkPushConstantRange pcr = {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 16};
      VkPipelineLayoutCreateInfo plci = {
         .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .setLayoutCount = 1,
         .pSetLayouts = &sl, .pushConstantRangeCount = 1, .pPushConstantRanges = &pcr};
      CK(vkCreatePipelineLayout(t.dev, &plci, NULL, &t.layout), "Layout");
      VkDescriptorPoolSize ps = {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1};
      VkDescriptorPoolCreateInfo pci = {
         .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .maxSets = 1,
         .poolSizeCount = 1, .pPoolSizes = &ps};
      VkDescriptorPool dp;
      CK(vkCreateDescriptorPool(t.dev, &pci, NULL, &dp), "Pool");
      VkDescriptorSetAllocateInfo ai = {
         .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, .descriptorPool = dp,
         .descriptorSetCount = 1, .pSetLayouts = &sl};
      CK(vkAllocateDescriptorSets(t.dev, &ai, &set), "Set");
      VkBufferViewCreateInfo bvci = {
         .sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO, .buffer = vb,
         .format = VK_FORMAT_R32G32B32A32_SFLOAT, .range = VK_WHOLE_SIZE};
      VkBufferView bv;
      CK(vkCreateBufferView(t.dev, &bvci, NULL, &bv), "View");
      VkWriteDescriptorSet w = {
         .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = set,
         .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
         .pTexelBufferView = &bv};
      vkUpdateDescriptorSets(t.dev, 1, &w, 0, NULL);
   }

   struct dx7_pipe_desc d = {
      .vs = pull ? (const uint32_t *)vpull_vert_spv : (const uint32_t *)s_vert_spv,
      .vs_size = pull ? vpull_vert_spv_len : s_vert_spv_len,
      .fs = (const uint32_t *)s_frag_spv, .fs_size = s_frag_spv_len,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, .cull_mode = VK_CULL_MODE_NONE,
      .no_vertex_input = (int)pull};
   VkPipeline p = dx7_pipeline(&t, &d);
   dx7_run(&t, p, vb, rec, NULL);
   const uint8_t *px = dx7_px(&t, RT_W / 2, RT_H / 2);
   int drawn = 0;
   for (int y = 0; y < RT_H; y += 8)
      for (int x = 0; x < RT_W; x += 8) if (!dx7_is_blue(dx7_px(&t, x, y))) drawn++;
   (void)px;
   struct timespec a, b2; clock_gettime(CLOCK_MONOTONIC, &a);
   for (int f = 0; f < frames; f++) dx7_run(&t, p, vb, rec, NULL);
   clock_gettime(CLOCK_MONOTONIC, &b2);
   double ms = ((b2.tv_sec - a.tv_sec) * 1e3 + (b2.tv_nsec - a.tv_nsec) / 1e6) / frames;
   printf("%s pull=%u indirect=%d noindex=%d pad=%u draws=%u indices/draw=%u window=%u: %.2f ms/frame, %.3f ms/draw (%d samples drawn)\n",
          drawn ? "PASS" : "FAIL", pull, indirect, noindex, ib_pad, ndraws, per, window, ms, ms / ndraws, drawn);
   return drawn ? 0 : 1;
}
