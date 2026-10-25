/* Tiler stress: argv: icd ntris ndraws frames [big]  (RT 1600x900). */
#define RT_W 1600
#define RT_H 900
#include "dx7_harness.h"
#include "spv.h"
static uint32_t ntris, ndraws;
static void rec(struct dx7 *t, VkCommandBuffer cmd, void *data) {
   uint32_t per = ntris / ndraws;
   for (uint32_t i = 0; i < ndraws; i++) vkCmdDraw(cmd, per * 3, 1, i * per * 3, 0);
}
int main(int argc, char **argv) {
   if (argc < 5) { printf("usage\n"); return 2; }
   ntris = atoi(argv[2]); ndraws = atoi(argv[3]); int frames = atoi(argv[4]);
   float big = argc > 5 ? atof(argv[5]) : 0.5f;
   struct dx7 t; VkPhysicalDeviceFeatures want = {0};
   dx7_init(&t, argv[1], &want);
   float *v = malloc((size_t)ntris * 3 * 16); srand(1);
   for (uint32_t i = 0; i < ntris; i++) {
      float cx = rand() / (float)RAND_MAX * 2 - 1, cy = rand() / (float)RAND_MAX * 2 - 1;
      for (int k = 0; k < 3; k++) {
         float *q = v + ((size_t)i * 3 + k) * 4;
         q[0] = cx + (rand() / (float)RAND_MAX - 0.5f) * big * 2;
         q[1] = cy + (rand() / (float)RAND_MAX - 0.5f) * big * 2;
         q[2] = 0.5f; q[3] = (float)i;
      }
   }
   VkBuffer vb; VkDeviceMemory vm;
   dx7_buffer(&t, (VkDeviceSize)ntris * 3 * 16, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, v, &vb, &vm);
   struct dx7_pipe_desc d = {.vs = (const uint32_t *)s_vert_spv, .vs_size = s_vert_spv_len,
      .fs = (const uint32_t *)s_frag_spv, .fs_size = s_frag_spv_len,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, .cull_mode = VK_CULL_MODE_NONE};
   VkPipeline p = dx7_pipeline(&t, &d);
   struct timespec a, b; clock_gettime(CLOCK_MONOTONIC, &a);
   for (int f = 0; f < frames; f++) dx7_run(&t, p, vb, rec, NULL);
   clock_gettime(CLOCK_MONOTONIC, &b);
   double ms = (b.tv_sec - a.tv_sec) * 1e3 + (b.tv_nsec - a.tv_nsec) / 1e6;
   const uint8_t *px = dx7_px(&t, 800, 450);
   printf("PASS ntris=%u ndraws=%u frames=%d ms/frame=%.2f px=%u,%u,%u\n", ntris, ndraws, frames, ms / frames, px[0], px[1], px[2]);
   return 0;
}
