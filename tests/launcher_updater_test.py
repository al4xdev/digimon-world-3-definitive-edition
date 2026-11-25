#!/usr/bin/env python3
"""dw2003-updater (launcher/updater.c) against a fake release on a local HTTP server: a good update, and every way it must
refuse or undo one. Linux; needs gcc and curl. Usage: python3 tests/launcher_updater_test.py [--updater PATH]
Without --updater it builds launcher/updater.c into a temp folder."""
import hashlib, http.server, os, shutil, stat, subprocess, sys, tempfile, threading, time, zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
fails = 0


def check(ok, what):
    global fails
    print(("ok    " if ok else "FAIL  ") + what)
    fails += 0 if ok else 1


def build(out):
    mz = ["-DMINIZ_NO_DEFLATE_APIS", "-DMINIZ_NO_TIME"]
    subprocess.run(["gcc", "-std=gnu99", "-D_GNU_SOURCE", "-O1", "-Wall", "-I" + ROOT + "/launcher",
                    ROOT + "/launcher/updater.c", ROOT + "/launcher/sha256.c",
                    ROOT + "/launcher/third_party/miniz/miniz.c", *mz, "-o", out], check=True,
                   stderr=subprocess.DEVNULL)


STUB_OK = "#!/bin/sh\n[ \"$1\" = --version ] && exit 0\necho started-new >> \"$(dirname \"$0\")/started.txt\"\n"
STUB_BAD = "#!/bin/sh\nexit 3\n"



HARNESS = r"""
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "cfg.h"
#include "cloud.h"
#include "selfupdate.h"
long plat_now_ms(void) { return 0; }
void plat_desktop_size(int *w, int *h) { *w = 640; *h = 480; }
/* argv: exe_dir url sha size  [cancel]: the launcher's side of an update, as main.c drives it */
int main(int argc, char **argv) {
    UpdateInfo u; memset(&u, 0, sizeof(u));
    snprintf(u.asset_name, sizeof(u.asset_name), "x.AppImage");
    snprintf(u.asset_url, sizeof(u.asset_url), "%s", argv[2]);
    snprintf(u.asset_sha256, sizeof(u.asset_sha256), "%s", argv[3]);
    u.asset_size = atol(argv[4]);
    if (!selfupd_available(&u, argv[1])) { puts("NOT AVAILABLE"); return 2; }
    char err[96];
    if (selfupd_start(&u, argv[1], err, sizeof(err)) != 0) { puts(err); return 3; }
    SelfUpdate su;
    for (int i = 0; i < 400; i++) {
        selfupd_poll(&su);
        if (su.state == SU_READY || su.state == SU_FAILED) break;
        usleep(50000);
    }
    printf("STATE %d %s\n", su.state, su.text);
    fflush(stdout);
    if (argc > 5) { selfupd_cancel(); puts("CANCELLED"); return 0; }
    if (su.state != SU_READY) return 4;
    selfupd_go();
    return 0; /* the launcher ends: the updater takes over */
}
"""


def zip_of(path, entries):
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as z:
        for name, data in entries.items():
            z.writestr(name, data)
    return hashlib.sha256(open(path, "rb").read()).hexdigest(), os.path.getsize(path)


class Quiet(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *a):
        pass


def serve(directory):
    h = lambda *a, **k: Quiet(*a, directory=directory, **k)
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), h)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    return srv


def tree(d):
    out = {}
    for r, _, fs in os.walk(d):
        for f in fs:
            p = os.path.join(r, f)
            out[os.path.relpath(p, d)] = open(p, "rb").read()
    return out


def status(p):
    try:
        return open(p).read().split(" ", 2)[0]
    except OSError:
        return ""


def wait_status(p, want, secs=20):
    end = time.time() + secs
    while time.time() < end:
        if status(p) in want:
            return status(p)
        time.sleep(0.05)
    return status(p)


_n = [0]


def clean(t):
    """A tree without the updater's own working files."""
    return {k: v for k, v in t.items() if not k.startswith((".update", "update.log", "started"))}


def install(tmp, name):
    _n[0] += 1
    d = os.path.join(tmp, "%s-%d" % (name, _n[0]))
    os.makedirs(d + "/mods")
    open(d + "/dw2003-launcher", "w").write("#!/bin/sh\necho OLD\n")
    os.chmod(d + "/dw2003-launcher", 0o755)
    open(d + "/README.txt", "w").write("old readme")
    open(d + "/mods/user_mod.txt", "w").write("the player's own")
    open(d + "/settings.json", "w").write("{\"player\":true}")
    open(d + "/card1.mcd", "wb").write(os.urandom(4096))
    return d


def run(upd, tmp, inst, url, sha, size, kind="zip", smoke="dw2003-launcher", extra=(), target=None):
    work, backup, st = inst + "/.update", inst + "/backup", inst + "/.update.status"
    os.makedirs(work, exist_ok=True)
    sleeper = subprocess.Popen(["sleep", "300"])  # stands for the running launcher
    args = [upd, "--kind", kind, "--url", url, "--sha256", sha, "--size", str(size), "--dir", inst, "--work", work,
            "--backup", backup, "--status", st, "--pid", str(sleeper.pid), "--restart", inst + "/dw2003-launcher"]
    if smoke:
        args += ["--smoke", smoke]
    if target:
        args += ["--target", target]
    p = subprocess.Popen(args + list(extra))
    return p, sleeper, work, st


def main():
    tmp = tempfile.mkdtemp(prefix="dw3-updater-test-")
    upd = os.path.join(tmp, "updater")
    if "--updater" in sys.argv:
        upd = sys.argv[sys.argv.index("--updater") + 1]
    else:
        build(upd)
    web = os.path.join(tmp, "web")
    os.makedirs(web)
    base = {"pkg/dw2003-launcher": STUB_OK, "pkg/README.txt": "new readme", "pkg/mods/m.json": "{}",
            "pkg/dw2003-updater": "new updater"}
    sha, size = zip_of(web + "/good.zip", base)
    srv = serve(web)
    url = "http://127.0.0.1:%d/" % srv.server_address[1]

    # 1. a good update: stages and waits, swaps only after the go-ahead and the launcher's end
    inst = install(tmp, "i1")
    os.makedirs(inst + "/backup"); open(inst + "/backup/stale.txt", "w").write("from the update before")
    before = tree(inst)
    p, sl, work, st = run(upd, tmp, inst, url + "good.zip", sha, size)
    check(wait_status(st, ["READY"]) == "READY", "good: reaches READY")
    check(tree(inst).get("README.txt") == b"old readme", "good: nothing replaced while waiting")
    open(work + "/update.go", "w").write("go")
    time.sleep(0.6)
    check(p.poll() is None, "good: still waits while the launcher's process lives")
    sl.kill(); sl.wait()
    p.wait(timeout=20)
    after = tree(inst)
    check(p.returncode == 0 and status(st) == "DONE", "good: DONE, exit 0")
    check(after["README.txt"] == b"new readme" and after["mods/m.json"] == b"{}", "good: the new files are in place")
    check(after["settings.json"] == before["settings.json"] and after["card1.mcd"] == before["card1.mcd"]
          and after["mods/user_mod.txt"] == before["mods/user_mod.txt"], "good: the player's files untouched")
    check(after["backup/README.txt"] == b"old readme" and "backup/stale.txt" not in after, "good: old files in backup/, the older backup gone")
    check(os.access(inst + "/dw2003-launcher", os.X_OK), "good: the program keeps its execute bit")
    time.sleep(0.5)
    check(os.path.exists(inst + "/started.txt"), "good: the new launcher was started")
    # manual rollback
    r = subprocess.run([upd, "--rollback", "--dir", inst, "--backup", inst + "/backup"], capture_output=True)
    check(r.returncode == 0 and open(inst + "/README.txt").read() == "old readme", "rollback by hand restores the old files")

    # 2. wrong hash, wrong size: nothing touched
    for what, s2, z2 in (("wrong sha256", "0" * 64, size), ("wrong size", sha, size + 1)):
        inst = install(tmp, "i2")
        before = tree(inst)
        p, sl, work, st = run(upd, tmp, inst, url + "good.zip", s2, z2)
        p.wait(timeout=20); sl.kill(); sl.wait()
        check(p.returncode == 1 and status(st) == "FAILED" and clean(tree(inst)) == clean(before) and not os.path.exists(inst + "/backup"), "%s: FAILED, install untouched" % what)

    # 3. a download that is not there
    inst = install(tmp, "i3")
    p, sl, work, st = run(upd, tmp, inst, url + "missing.zip", sha, size)
    p.wait(timeout=30); sl.kill(); sl.wait()
    check(p.returncode == 1 and status(st) == "FAILED", "404: FAILED")

    # 4. zip-slip
    for evil in ("pkg/../../evil.txt", "/abs.txt", "pkg/a/../../../x"):
        sha4, size4 = zip_of(web + "/evil.zip", {"pkg/dw2003-launcher": STUB_OK, evil: "x"})
        inst = install(tmp, "i4")
        p, sl, work, st = run(upd, tmp, inst, url + "evil.zip", sha4, size4)
        p.wait(timeout=20); sl.kill(); sl.wait()
        outside = [f for f in os.listdir(tmp) if f in ("evil.txt", "abs.txt", "x")]
        check(p.returncode == 1 and status(st) == "FAILED" and not outside, "zip-slip %r: refused, nothing written outside" % evil)

    # 5. the staged launcher does not start
    sha5, size5 = zip_of(web + "/bad.zip", {"pkg/dw2003-launcher": STUB_BAD, "pkg/README.txt": "x"})
    inst = install(tmp, "i5")
    p, sl, work, st = run(upd, tmp, inst, url + "bad.zip", sha5, size5)
    p.wait(timeout=30); sl.kill(); sl.wait()
    check(status(st) == "FAILED" and open(inst + "/README.txt").read() == "old readme", "smoke: a launcher that fails --version is not installed")

    # 6. a failure in the middle of the swap puts everything back
    many = {"pkg/dw2003-launcher": STUB_OK}
    for i in range(12):
        many["pkg/f%02d.txt" % i] = "new %d" % i
    many["pkg/sub/inner.txt"] = "new"
    sha6, size6 = zip_of(web + "/many.zip", many)
    inst = install(tmp, "i6")
    for i in range(12):
        open(inst + "/f%02d.txt" % i, "w").write("old %d" % i)
    open(inst + "/sub", "w").write("a FILE where the package wants a folder")
    before = tree(inst)
    p, sl, work, st = run(upd, tmp, inst, url + "many.zip", sha6, size6)
    wait_status(st, ["READY"]); open(work + "/update.go", "w").write("go"); sl.kill(); sl.wait()
    p.wait(timeout=30)
    after = {k: v for k, v in clean(tree(inst)).items() if not k.startswith("backup/")}
    want = clean(before)
    check(status(st) == "FAILED" and after == want, "swap failure: every file is back as it was")
    time.sleep(0.4)

    # 7. the launcher ends without the go-ahead: staged, nothing changed
    inst = install(tmp, "i7")
    before = tree(inst)
    p, sl, work, st = run(upd, tmp, inst, url + "good.zip", sha, size)
    wait_status(st, ["READY"]); sl.kill(); sl.wait()
    p.wait(timeout=20)
    check(p.returncode == 0 and open(inst + "/README.txt").read() == "old readme" and not os.path.exists(inst + "/backup"), "no go-ahead: nothing changed")

    # 8. a single file (the AppImage): replaces the one named, executable, the old one in backup/
    img = b"\x7fELF" + os.urandom(2000)
    open(web + "/new.AppImage", "wb").write(img)
    shaf = hashlib.sha256(img).hexdigest()
    inst = install(tmp, "i8")
    open(inst + "/game.AppImage", "wb").write(b"old image"); os.chmod(inst + "/game.AppImage", 0o755)
    p, sl, work, st = run(upd, tmp, inst, url + "new.AppImage", shaf, len(img), kind="file", smoke=None, target="game.AppImage")
    wait_status(st, ["READY"]); open(work + "/update.go", "w").write("go"); sl.kill(); sl.wait()
    p.wait(timeout=20)
    check(open(inst + "/game.AppImage", "rb").read() == img and os.access(inst + "/game.AppImage", os.X_OK)
          and open(inst + "/backup/game.AppImage", "rb").read() == b"old image", "file kind: replaced, executable, the old in backup/")

    # 9. the launcher's side (selfupdate.c) driving the real updater, the AppImage way: ready, go, the launcher ends
    h = os.path.join(tmp, "harness")
    open(h + ".c", "w").write(HARNESS)
    subprocess.run(["gcc", "-std=gnu99", "-D_GNU_SOURCE", "-Wall", "-DDW3_TARGET_PC", "-I" + ROOT + "/launcher", h + ".c",
                    ROOT + "/launcher/selfupdate.c", ROOT + "/launcher/cloud.c", ROOT + "/launcher/cfg.c",
                    ROOT + "/launcher/sha256.c", ROOT + "/launcher/migrate.c", "-o", h, "-lm", "-lpthread"], check=True, stderr=subprocess.DEVNULL)
    for mode in ("go", "cancel"):
        base = os.path.join(tmp, "ai-" + mode)
        os.makedirs(base + "/usr/bin"); os.makedirs(base + "/home")
        shutil.copy(upd, base + "/usr/bin/dw2003-updater")
        app = base + "/home/game.AppImage"
        open(app, "w").write("#!/bin/sh\n[ \"$1\" = --version ] && exit 0\n")
        os.chmod(app, 0o755)
        newimg = ("#!/bin/sh\n[ \"$1\" = --version ] && exit 0\necho new-started > " + base + "/home/started.txt\n").encode()
        open(web + "/ai.AppImage", "wb").write(newimg)
        env = dict(os.environ, APPIMAGE=app)
        args = [h, base + "/usr/bin", url + "ai.AppImage", hashlib.sha256(newimg).hexdigest(), str(len(newimg))]
        r = subprocess.run(args + (["cancel"] if mode == "cancel" else []), env=env, capture_output=True, text=True, timeout=60)
        check("STATE 2" in r.stdout, "launcher side (%s): the updater reached READY" % mode)
        time.sleep(1.5)
        if mode == "go":
            check(open(app, "rb").read() == newimg and os.path.exists(base + "/home/started.txt")
                  and os.path.exists(base + "/home/.dw2003-backup/game.AppImage"), "launcher side: after the launcher ended the AppImage was replaced, the old kept, the new one started")
            check(os.path.exists(base + "/home/.dw2003-update/run/dw2003-updater"), "launcher side: the updater ran from a copy outside the AppImage")
        else:
            check(open(app).read().startswith("#!/bin/sh") and "CANCELLED" in r.stdout and not os.path.exists(base + "/home/.dw2003-update"), "launcher side: cancel leaves the AppImage as it was and removes the staging")

    srv.shutdown()
    shutil.rmtree(tmp, ignore_errors=True)
    print("\n%s" % ("all passed" if not fails else "%d FAILED" % fails))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
