"""Plugin host smoke test.

Drives YI3D.exe over IPC: NewFile -> NewSketch3D (async env switch, retried)
-> execute the demo plugin command -> assert on the plugin host log.

Usage:
    python plugin_smoke.py [--exe <path to YI3D.exe>]
"""

import argparse
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from run_script import ipc_request, wait_for_ipc, is_ok, get_message, kill_yi3d

PROJECT_DIR = Path(__file__).resolve().parent.parent.parent
DEMO_COMMAND = "com.yi3d.demo.hello"
LOG_REL_PATH = Path("plugins") / "plugin-manager.log"


def read_log(exe_dir: Path) -> str:
    log_path = exe_dir / LOG_REL_PATH
    if not log_path.exists():
        return ""
    return log_path.read_text(encoding="utf-8", errors="replace")


def run_command(command: str, attempts: int = 10) -> str:
    """Execute a command over IPC, retrying while the environment settles."""
    resp = ""
    for _ in range(attempts):
        resp = ipc_request("command", command, timeout=30.0)
        if is_ok(resp):
            return resp
        time.sleep(0.5)
    return resp


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", default=str(PROJECT_DIR / "out" / "Release" / "YI3D.exe"))
    args = parser.parse_args()

    exe_path = Path(args.exe).resolve()
    if not exe_path.exists():
        print(f"FAIL: exe not found: {exe_path}")
        return 1
    exe_dir = exe_path.parent

    # Start from a fresh log so assertions cannot match an older session
    log_path = exe_dir / LOG_REL_PATH
    if log_path.exists():
        log_path.unlink()

    failures = []
    kill_yi3d()
    time.sleep(1.0)

    proc = subprocess.Popen([str(exe_path)], cwd=str(exe_dir),
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        if not wait_for_ipc(timeout=30.0):
            failures.append("IPC not ready (timeout)")
        else:
            time.sleep(1.0)

            resp = ipc_request("command", "NewFile", timeout=30.0)
            if not is_ok(resp):
                failures.append(f"NewFile failed: {get_message(resp)}")

            # NewSketch3D may race with the asynchronous document-environment
            # transition after NewFile; retry until the modeling env accepts it.
            resp = run_command("NewSketch3D")
            if not is_ok(resp):
                failures.append(f"NewSketch3D failed: {get_message(resp)}")

            time.sleep(1.0)

            # The sketch3d environment may still be entering; retry.
            resp = run_command(DEMO_COMMAND)
            if not is_ok(resp):
                failures.append(f"demo command failed: {get_message(resp)}")

            # Exit and re-enter the environment: the plugin command and UI
            # must be re-attached (teardown/re-add cycle).
            resp = run_command("CancelSketch3D")
            if not is_ok(resp):
                failures.append(f"CancelSketch3D failed: {get_message(resp)}")
            time.sleep(1.0)

            resp = run_command("NewSketch3D")
            if not is_ok(resp):
                failures.append(f"NewSketch3D (re-enter) failed: {get_message(resp)}")
            time.sleep(1.0)

            resp = run_command(DEMO_COMMAND)
            if not is_ok(resp):
                failures.append(f"demo command after re-enter failed: {get_message(resp)}")
    finally:
        time.sleep(0.5)
        kill_yi3d()
        try:
            proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            pass

    log = read_log(exe_dir)

    def expect(condition: bool, message: str) -> None:
        if not condition:
            failures.append(message)

    expect("loaded plugin 'com.yi3d.demo'." in log, "log misses the loaded-plugin line")
    expect("plugin host: 1 plugin(s): 1 loaded, 0 disabled, 0 failed." in log, "log misses the summary line")
    expect("[com.yi3d.demo] demo plugin initialized" in log, "log misses the plugin init line")
    expect(log.count("ui built for environment 'sketch3d'") >= 2, "ui not rebuilt after re-entering sketch3d")
    expect("[com.yi3d.demo] demo ui torn down" in log, "log misses the ui teardown line")
    expect(log.count("hello command executed") >= 2, "hello did not run after re-entering sketch3d")

    if failures:
        print("FAIL")
        for failure in failures:
            print(f"  - {failure}")
        print("--- plugin host log ---")
        print(log if log else "(no log file)")
        return 1

    print("PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
