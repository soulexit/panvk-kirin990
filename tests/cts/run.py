#!/usr/bin/env python3
"""Host-side dEQP-VK runner over adb (no root, no deqp-runner).

run.py <caselist> <outdir> [-j N] [-b batch] [--stall secs] [--serial S]

Runs the cases in batches on the device, streaming stdout. A case that starts
and never reports a result is recorded as Crash (process died) or Timeout (no
output for --stall seconds); the batch is then resumed after that case.
Results go to <outdir>/results.csv (case,status,detail), appended as they come,
so an interrupted run can be resumed by re-running the same command.
"""
import argparse, csv, os, queue, re, subprocess, sys, threading, time

DEV = "/data/local/tmp/cts"
STATUS = re.compile(r"^\s+(Pass|Fail|NotSupported|QualityWarning|CompatibilityWarning|"
                    r"InternalError|ResourceError|Crash|Timeout|Waiver)\s*\((.*)\)\s*$")
START = re.compile(r"^Test case '(dEQP-VK\.[^']+)'")

ap = argparse.ArgumentParser()
ap.add_argument("caselist"); ap.add_argument("outdir")
ap.add_argument("-j", type=int, default=2)
ap.add_argument("-b", type=int, default=300)
ap.add_argument("--stall", type=int, default=90)
ap.add_argument("--serial", default=os.environ.get("ANDROID_SERIAL", ""))
ap.add_argument("--icd", default="libvulkan_panfrost.so")
ap.add_argument("--env", default="")
ap.add_argument("--vmem-kb", type=int, default=16 * 1024 * 1024)
a = ap.parse_args()

adb = ["adb"] + (["-s", a.serial] if a.serial else [])
os.makedirs(a.outdir, exist_ok=True)
res_path = os.path.join(a.outdir, "results.csv")
done = set()
if os.path.exists(res_path):
    with open(res_path) as f:
        done = {r[0] for r in csv.reader(f) if r}
cases = [l.strip() for l in open(a.caselist) if l.strip() and l.strip() not in done]
print(f"{len(cases)} cases to run ({len(done)} already done)", flush=True)

lock = threading.Lock()
resf = open(res_path, "a", newline="")
resw = csv.writer(resf)
counts = {}
work = queue.Queue()
for i in range(0, len(cases), a.b):
    work.put(cases[i:i + a.b])


def record(case, status, detail=""):
    with lock:
        resw.writerow([case, status, detail[:200]]); resf.flush()
        counts[status] = counts.get(status, 0) + 1
        if status not in ("Pass", "NotSupported"):
            print(f"{status:12s} {case}  {detail[:100]}", flush=True)


def run_batch(wid, batch):
    lst = f"{DEV}/{os.path.basename(os.path.normpath(a.outdir))}-w{wid}.txt"
    p = subprocess.run(adb + ["shell", f"cat > {lst}"], input="\n".join(batch) + "\n",
                       text=True)
    cmd = (f"cd {DEV} && ulimit -v {a.vmem_kb}; VKSHIM_ICD={DEV}/{a.icd} {a.env} "
           f"MESA_SHADER_CACHE_DISABLE=1 ./deqp-vk --deqp-caselist-file={lst} "
           f"--deqp-log-filename=/dev/null --deqp-log-images=disable "
           f"--deqp-log-shader-sources=disable --deqp-shadercache=disable "
           f"--deqp-vk-library-path={DEV}/libvkshim.so --deqp-archive-dir={DEV} "
           f"--deqp-terminate-on-device-lost=enable 2>&1; echo __RC=$?")
    proc = subprocess.Popen(adb + ["shell", cmd], stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors="replace")
    last = [time.time()]
    cur, seen = None, set()
    tail = []

    def watchdog():
        while proc.poll() is None:
            if time.time() - last[0] > a.stall:
                subprocess.run(adb + ["shell", f"pkill -f 'caselist-file={lst}'"])
                proc.kill()
                return
            time.sleep(2)
    threading.Thread(target=watchdog, daemon=True).start()
    timed_out = False
    for line in proc.stdout:
        last[0] = time.time()
        m = START.match(line)
        if m:
            cur = m.group(1); tail = []; continue
        m = STATUS.match(line)
        if m and cur:
            record(cur, m.group(1), m.group(2)); seen.add(cur); cur = None; continue
        if cur or "FATAL" in line or "error" in line.lower():
            tail = (tail + [line.strip()])[-3:]
    proc.wait()
    if time.time() - last[0] > a.stall:
        timed_out = True
    if cur:
        record(cur, "Timeout" if timed_out else "Crash", " | ".join(tail))
        seen.add(cur)
    elif not seen:
        # process died before starting any case: blame the first one so we make progress
        record(batch[0], "Crash", "no output | " + " | ".join(tail))
        seen.add(batch[0])
    rest = [c for c in batch if c not in seen]
    if rest:
        # cases before the crashing one that never started are re-queued
        work.put(rest)


def worker(wid):
    while True:
        try:
            batch = work.get(timeout=5)
        except queue.Empty:
            return
        run_batch(wid, batch)


t0 = time.time()
ths = [threading.Thread(target=worker, args=(w,)) for w in range(a.j)]
for t in ths: t.start()
for t in ths: t.join()
print(f"done in {time.time() - t0:.0f}s: {counts}", flush=True)
