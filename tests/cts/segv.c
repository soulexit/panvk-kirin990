/* LD_PRELOAD: on SIGSEGV print pc/lr/x29-chain as module+offset (dladdr), then die. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <ucontext.h>
#include <unistd.h>

static void where(const char *tag, uintptr_t a)
{
   Dl_info i;
   if (a && dladdr((void *)a, &i) && i.dli_fname)
      fprintf(stderr, "SEGV %s %s+0x%lx (%s)\n", tag, i.dli_fname, (unsigned long)(a - (uintptr_t)i.dli_fbase),
              i.dli_sname ? i.dli_sname : "?");
   else
      fprintf(stderr, "SEGV %s 0x%lx\n", tag, (unsigned long)a);
}

static void h(int sig, siginfo_t *si, void *uc_)
{
   ucontext_t *uc = uc_;
   fprintf(stderr, "SEGV addr %p\n", si->si_addr);
   where("pc", uc->uc_mcontext.pc);
   where("lr", uc->uc_mcontext.regs[30]);
   uintptr_t *fp = (uintptr_t *)uc->uc_mcontext.regs[29];
   for (int k = 0; k < 16 && fp && ((uintptr_t)fp & 7) == 0; k++) {
      where("fp", fp[1]);
      fp = (uintptr_t *)fp[0];
   }
   _exit(139);
}

__attribute__((constructor)) static void init(void)
{
   struct sigaction sa = {0};
   sa.sa_sigaction = h; sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
   sigaction(SIGSEGV, &sa, NULL);
}
