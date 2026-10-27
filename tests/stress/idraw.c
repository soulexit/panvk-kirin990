/* Indexed-draw cost: argv: icd ndraws indices_per_draw vertex_window frames [mode]
 * Each draw uses indices_per_draw indices (small triangles) referring to vertices in
 * a window of vertex_window vertices of one shared vertex buffer, like DXVK packing
 * many meshes into one buffer. mode: 0 = vkCmdDrawIndexed (default), 1 = vkCmdDraw
 * of the same triangle count (no index range work), 2 = one vkCmdDrawIndexed per
 * frame with all indices. Prints ms per frame and per draw. RT 1280x720. */
#define RT_W 1280
#define RT_H 720
#include "dx7_harness.h"
#include "spv.h"
static uint32_t ndraws, per, mode;
static VkBuffer ib;
static void rec(struct dx7 *t, VkCommandBuffer cmd, void *data) {
   if (mode == 1) {
      for (uint32_t i = 0; i < ndraws; i++) vkCmdDraw(cmd, per, 1, i * per, 0);
      return;
   }
   vkCmdBindIndexBuffer(cmd, ib, 0, VK_INDEX_TYPE_UINT32);
   if (mode == 2) {
      vkCmdDrawIndexed(cmd, per * ndraws, 1, 0, 0, 0);
      return;
   }
   for (uint32_t i = 0; i < ndraws; i++) vkCmdDrawIndexed(cmd, per, 1, i * per, 0, 0);
}
int main(int argc, char **argv) {
   if (argc < 6) { printf("usage: icd ndraws indices_per_draw vertex_window frames [mode]\n"); return 2; }
   ndraws = atoi(argv[2]); per = atoi(argv[3]) / 3 * 3;
   const uint32_t window = atoi(argv[4]); const int frames = atoi(argv[5]);
   mode = argc > 6 ? atoi(argv[6]) : 0;
   struct dx7 t; VkPhysicalDeviceFeatures want = {0};
   dx7_init(&t, argv[1], &want);

   /* Vertex buffer: ndraws windows of `window` vertices, tiny triangles. */
   const uint32_t nverts = mode == 1 ? ndraws * per : ndraws * window;
   float *v = malloc((size_t)nverts * 16); srand(1);
   for (uint32_t i = 0; i < nverts; i++) {
      float *q = v + (size_t)i * 4;
      q[0] = rand() / (float)RAND_MAX * 2 - 1; q[1] = rand() / (float)RAND_MAX * 2 - 1;
      q[2] = 0.5f; q[3] = (float)i;
   }
   { /* consecutive vertex triples are tiny triangles */
      for (uint32_t i = 0; i + 2 < nverts; i += 3)
         for (int k = 1; k < 3; k++) {
            v[(i + k) * 4 + 0] = v[i * 4 + 0] + 0.004f * k;
            v[(i + k) * 4 + 1] = v[i * 4 + 1] + 0.004f * (k == 2);
         }
   }
   VkBuffer vb; VkDeviceMemory vm;
   dx7_buffer(&t, (VkDeviceSize)nverts * 16, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, v, &vb, &vm);

   /* Index buffer: draw d references whole triangles (vertex triples) of its
    * window [d*window, (d+1)*window), in random order. */
   uint32_t *idx = malloc((size_t)ndraws * per * 4);
   const uint32_t tris_in_window = window / 3 ? window / 3 : 1;
   for (uint32_t d = 0; d < ndraws; d++)
      for (uint32_t i = 0; i < per; i += 3) {
         /* IDRAW_SEQ=1: triangles in order (best vertex locality). */
         const uint32_t tri = getenv("IDRAW_SEQ") ? (i / 3) % tris_in_window
                                                  : (uint32_t)(rand() % tris_in_window);
         const uint32_t base = d * window + tri * 3;
         for (int k = 0; k < 3; k++)
            idx[(size_t)d * per + i + k] = base + k;
      }
   VkDeviceMemory im;
   dx7_buffer(&t, (VkDeviceSize)ndraws * per * 4, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, idx, &ib, &im);

   struct dx7_pipe_desc d = {.vs = (const uint32_t *)s_vert_spv, .vs_size = s_vert_spv_len,
      .fs = (const uint32_t *)s_frag_spv, .fs_size = s_frag_spv_len,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, .cull_mode = VK_CULL_MODE_NONE};
   /* IDRAW_DISCARD=1: vertex work only (no tiler); IDRAW_CULL=1: every
    * triangle culled by the tiler (no rasterisation). */
   if (getenv("IDRAW_DISCARD")) d.rasterizer_discard = 1;
   if (getenv("IDRAW_CULL")) d.cull_mode = VK_CULL_MODE_FRONT_AND_BACK;
   VkPipeline p = dx7_pipeline(&t, &d);
   dx7_run(&t, p, vb, rec, NULL); /* warm-up: shader compile, first allocations */
   struct timespec a, b; clock_gettime(CLOCK_MONOTONIC, &a);
   for (int f = 0; f < frames; f++) dx7_run(&t, p, vb, rec, NULL);
   clock_gettime(CLOCK_MONOTONIC, &b);
   double ms = ((b.tv_sec - a.tv_sec) * 1e3 + (b.tv_nsec - a.tv_nsec) / 1e6) / frames;
   printf("PASS mode=%u draws=%u indices/draw=%u window=%u: %.2f ms/frame, %.3f ms/draw\n",
          mode, ndraws, per, window, ms, ms / (mode == 2 ? 1 : ndraws));
   return 0;
}
