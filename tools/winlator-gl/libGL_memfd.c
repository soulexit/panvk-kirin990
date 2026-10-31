/* libGL_memfd.so: Winlator's libGL.so.1 (Mesa, Zink) needs libxshmfence, which
 * calls memfd_create(); bionic has it only from Android 11 (API 30), so on
 * Android 10 libGL fails to load and Wine disables OpenGL. This library provides
 * memfd_create() through the system call and has Winlator's libGL.so.1 as its
 * dependency: load it instead of libGL with BOX64_LIBGL=<path to this file>. The
 * symbol resolves from here (the root of the load), and box64's lookups of the
 * GL entry points reach the real libGL through the dependency. */
#define _GNU_SOURCE
#include <sys/syscall.h>
#include <unistd.h>

__attribute__((visibility("default"))) int
memfd_create(const char *name, unsigned int flags)
{
   return (int)syscall(__NR_memfd_create, name, flags);
}
