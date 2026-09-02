"""yi3d batch test launcher.

No valid args → interactive menu.
With valid --build and --directory → non-interactive.

Usage:
    python run_tests.py                                          # interactive
    python run_tests.py --build Release --directory *            # non-interactive
    python run_tests.py --build * --directory math               # non-interactive
"""

import time
import sys
from pathlib import Path

# ANSI colors
BOLD = "\033[1m"
MAGENTA = "\033[95m"
CYAN = "\033[96m"
GREEN = "\033[92m"
YELLOW = "\033[93m"
RED = "\033[91m"
RESET = "\033[0m"

from run_script import run_script, kill_yi3d

PROJECT_DIR = Path(__file__).resolve().parent.parent.parent
SCRIPTS_DIR = PROJECT_DIR / "scripts" / "samples"
OUT_DIR = PROJECT_DIR / "out"

VALID_BUILDS = {"Release", "Debug", "*"}


# ── folder discovery ───────────────────────────────────────────

def discover_folders() -> list[tuple[str, str]]:
    """Scan SCRIPTS_DIR for subdirectories containing .py files.
    Returns sorted list of (name, filter) tuples, with ALL first."""
    dirs: list[tuple[str, str]] = []

    for p in sorted(SCRIPTS_DIR.iterdir()):
        if p.is_dir() and not p.name.startswith("."):
            if list(p.glob("*.py")):
                dirs.append((p.name, p.name + "/"))

    return [("ALL", "")] + dirs


FOLDERS = discover_folders()
VALID_DIRS = {name for name, _ in FOLDERS} | {"*"}


# ── helpers ────────────────────────────────────────────────────

def parse_build(val: str) -> list[str]:
    """'*' → ['Release', 'Debug'], else single-element list."""
    if val == "*":
        return ["Release", "Debug"]
    return [val]


def parse_dir(val: str) -> str:
    """'*' → '', 'create' → 'create/'."""
    if val == "*":
        return ""
    if "/" not in val and "\\" not in val:
        return val + "/"
    return val


def validate_build(build: str) -> bool:
    return (OUT_DIR / build / "YI3D.exe").exists()


def collect_scripts(dir_arg: str) -> list[Path]:
    """Collect .py scripts matching dir_arg filter. Sorted by name."""
    scripts: list[Path] = []
    for folder_name, folder_filter in FOLDERS:
        if folder_name == "ALL":
            continue
        if dir_arg and folder_filter != dir_arg:
            continue
        folder = SCRIPTS_DIR / folder_name
        for py_file in sorted(folder.glob("*.py")):
            scripts.append(py_file)
    return scripts


# ── batch run ──────────────────────────────────────────────────

def run_one_build(exe_path: str, scripts: list[Path]) -> tuple[int, int]:
    """Run all scripts for one build. Returns (passed, failed)."""
    total = len(scripts)
    passed = 0
    failed = 0

    for i, script in enumerate(scripts):
        rel = str(script.relative_to(SCRIPTS_DIR))
        label = f"[{i + 1:3d}/{total}] {rel:<55s}"
        print(label, end=' ', flush=True)

        ok, msg = run_script(exe_path, str(script))
        if ok:
            passed += 1
            print(f"{GREEN}PASS{RESET} ({msg})")
        else:
            failed += 1
            short = msg[:80].replace('\n', ' ')
            print(f"{RED}FAIL{RESET} {short}")

        time.sleep(1.0)

    return passed, failed


def run_all(builds: list[str], dir_arg: str) -> bool:
    """Run tests across builds/folders. Returns True if all passed."""
    scripts = collect_scripts(dir_arg)
    if not scripts:
        print("No scripts found.")
        return True

    overall_ok = True
    multi = len(builds) > 1

    for build in builds:
        if multi:
            print(f"\n{'=' * 60}")
            print(f"  Testing: {build}")
            print(f"{'=' * 60}")

        exe = str(OUT_DIR / build / "YI3D.exe")
        if not validate_build(build):
            print(f"{RED}ERROR:{RESET} YI3D.exe not found at {exe}")
            overall_ok = False
            continue

        passed, failed = run_one_build(exe, scripts)

        print()
        print(f"  {build}: {GREEN}{passed} passed{RESET}, {RED}{failed} failed{RESET}, {len(scripts)} total")

        if failed > 0:
            overall_ok = False

    print(f"\n{'=' * 60}")
    if overall_ok:
        print(f"  {GREEN}ALL TESTS PASSED{RESET}")
    else:
        print(f"  {RED}SOME TESTS FAILED{RESET}")
    print("=" * 60)

    return overall_ok


# ── interactive menu ───────────────────────────────────────────

def input_number(prompt: str, min_n: int, max_n: int, default: int) -> int:
    """Prompt for a number in [min_n, max_n]. Re-prompt on invalid input."""
    while True:
        raw = input(prompt).strip()
        if raw == "":
            return default
        try:
            n = int(raw)
            if min_n <= n <= max_n:
                return n
        except ValueError:
            pass
        print(f"  Please enter a number between {min_n} and {max_n}.")


def interactive_menu():
    print("=" * 60)
    print("  YI3D Script Test Runner")
    print("=" * 60)

    build_label = "ALL"
    builds: list[str] = ["Release", "Debug"]
    folder_name = "ALL"
    folder_filter = ""

    while True:
        # --- choose build ---
        print()
        print(f">>> Select build type:")
        print("   [0] ALL")
        print("   [1] Release")
        print("   [2] Debug")
        print()
        n = input_number("Enter number [0-2, Enter=ALL]: ", 0, 2, 0)
        build_map = {0: ["Release", "Debug"], 1: ["Release"], 2: ["Debug"]}
        builds = build_map[n]
        build_label = "ALL" if n == 0 else builds[0]
        print(f"Build: {CYAN}{build_label}{RESET}")

        for b in builds:
            if not validate_build(b):
                print(f"{RED}ERROR:{RESET} YI3D.exe not found at {OUT_DIR / b / 'YI3D.exe'}")
                input("Press any key to exit...")
                return

        # --- choose folder ---
        print()
        print(f">>> Available test folders:")
        for i, (name, _) in enumerate(FOLDERS):
            label = "ALL (run all tests)" if i == 0 else name
            print(f"   [{i}] {label}")
        print()
        n = input_number(f"Enter number [0-{len(FOLDERS) - 1}, Enter=ALL]: ",
                         0, len(FOLDERS) - 1, 0)
        folder_name, folder_filter = FOLDERS[n]
        print(f"Folder: {CYAN}{folder_name}{RESET}")

        # --- confirm ---
        print()
        print(f"{'─' * 50}")
        print(f"{BOLD}Build:{RESET} {CYAN}{build_label}{RESET}    {BOLD}Folder:{RESET} {CYAN}{folder_name}{RESET}")
        print(f"{'─' * 50}")
        print()
        print(f">>> [0] Run    [1] Reselect    [2] Exit")
        print()
        n = input_number("Enter number [0-2, Enter=Run]: ", 0, 2, 0)
        if n == 0:
            break
        elif n == 2:
            print(f"  {YELLOW}Exited.{RESET}")
            return

    print()
    print("Each test: launch YI3D.exe > run script > kill YI3D.exe")
    print()

    # Kill leftovers
    kill_yi3d()
    time.sleep(1.0)

    run_all(builds, folder_filter)

    print()
    input("Press any key to exit...")


# ── entry ──────────────────────────────────────────────────────

def main():
    args = sys.argv[1:]

    # Parse --build and --directory
    build_val = None
    dir_val = None

    i = 0
    while i < len(args):
        if args[i] == "--build" and i + 1 < len(args):
            build_val = args[i + 1]
            i += 2
        elif args[i] == "--directory" and i + 1 < len(args):
            dir_val = args[i + 1]
            i += 2
        else:
            i += 1

    # Both must be present and valid for non-interactive mode
    if build_val is not None and dir_val is not None \
            and build_val in VALID_BUILDS and dir_val in VALID_DIRS:
        builds = parse_build(build_val)
        dir_arg = parse_dir(dir_val)

        kill_yi3d()
        time.sleep(1.0)

        ok = run_all(builds, dir_arg)
        sys.exit(0 if ok else 1)
    else:
        interactive_menu()


if __name__ == "__main__":
    main()
