#!/usr/bin/env python3
"""Huawei P40/HarmonyOS 4.2 device-wide rotation compatibility profile.

This intentionally does NOT change the working 72-byte JM atom layout.

A confirmed A/B test on Huawei ELS-AN00 showed that hiding the Vulkan
VK_KHR_swapchain_maintenance1 extension *or* disabling swapchain
pre-rotation in ARMSX2 independently prevents its rotation crash.
Until the driver-side WSI lifecycle is root-caused, hide the extension
on the affected Huawei Mali-G76 device in all Vulkan applications.

Reversible: PANVK_H42_ROTATION_COMPAT=0 restores original advertisement.
The existing PANVK_HIDE_EXTS list continues to work.
"""
from pathlib import Path
import sys

if len(sys.argv) != 2:
    raise SystemExit("Usage: apply_rotation_compat.py MESA/src/panfrost/vulkan/panvk_physical_device.c")
p = Path(sys.argv[1])
s = p.read_text()
anchor = "   panvk_hide_device_extensions(&supported_extensions);"
if s.count(anchor) != 1:
    raise SystemExit(f"Expected extension filter call exactly once; got {s.count(anchor)}")

# All extra includes are guarded for Android. Host Mesa builds are unaffected.
include = r"""
#if defined(__ANDROID__)
#include <strings.h>
#include <sys/system_properties.h>
#endif
"""
pos = s.find("#include ")
if pos < 0:
    raise SystemExit("Could not locate C includes")
s = s[:pos] + include + s[pos:]

# This helper is restricted to kbase, where the fork's extension filter lives.
helper = r"""
#if defined(__ANDROID__)
static bool
panvk_h42_is_huawei(void)
{
   char manufacturer[PROP_VALUE_MAX] = {0};
   if (__system_property_get("ro.product.manufacturer", manufacturer) <= 0 ||
       strcasecmp(manufacturer, "HUAWEI"))
      return false;

   return true;
}

static void
panvk_h42_rotation_compat(struct panvk_physical_device *device,
                          struct vk_device_extension_table *exts)
{
   /* Caller can disable this temporary compatibility workaround. */
   const char *override = getenv("PANVK_H42_ROTATION_COMPAT");
   if (override && override[0] == '0')
      return;

   /* Huawei Mali-G76 / Kirin 990, any Android Vulkan application.
    * Keep other devices unaffected. Opt out per app via environment. */
   if ((uint32_t)device->kmod.dev->props.gpu_id != 0x72110000u ||
       !panvk_h42_is_huawei())
      return;

   for (unsigned i = 0; i < VK_DEVICE_EXTENSION_COUNT; i++) {
      if (strcmp(vk_device_extensions[i].extensionName,
                 "VK_KHR_swapchain_maintenance1"))
         continue;
      if (exts->extensions[i]) {
         exts->extensions[i] = false;
         mesa_logi("panvk: H42 rotation compatibility: hiding "
                   "VK_KHR_swapchain_maintenance1 on Huawei G76 (all apps); "
                   "PANVK_H42_ROTATION_COMPAT=0 disables workaround");
      }
      break;
   }
}
#endif

"""
func="VkResult\npanvk_physical_device_init_kbase("
if s.count(func)!=1:
    raise SystemExit("Unexpected device init function anchor")
s=s.replace(func,helper+func,1)

s=s.replace(anchor, anchor+r"""
#if defined(__ANDROID__)
   panvk_h42_rotation_compat(device, &supported_extensions);
#endif""",1)
p.write_text(s)
print("Applied device-wide Huawei G76 rotation compatibility patch to",p)
