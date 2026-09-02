"""Execute a single script via YI3D.exe IPC.

Launches YI3D.exe, connects via TCP, creates a new document,
runs the script, then kills YI3D.exe.

Usage:
    python run_script.py --exe <path> <script.py>
"""

import subprocess
import socket
import struct
import json
import time
import sys
import os
from pathlib import Path

# --- IPC protocol ---

IPC_MAGIC = b'YI3D'
IPC_VERSION = 1
IPC_MSG_TYPE_REQUEST = 1
IPC_PORT = 17999
IPC_HOST = '127.0.0.1'


def build_frame(payload: bytes) -> bytes:
    header = struct.pack('>4sBBH I',
                         IPC_MAGIC, IPC_VERSION, IPC_MSG_TYPE_REQUEST,
                         0, len(payload))
    return header + payload


def ipc_request(cmd: str, argument: str, timeout: float = 60.0) -> str:
    """Open a connection, send one IPC request, read response, close."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(timeout)
    try:
        sock.connect((IPC_HOST, IPC_PORT))
    except (ConnectionRefusedError, OSError):
        return "ERROR|ConnectionRefused"

    try:
        req = json.dumps({"commnad": cmd, "argument": argument}, ensure_ascii=False)
        sock.sendall(build_frame(req.encode('utf-8')))

        header = b''
        while len(header) < 12:
            chunk = sock.recv(12 - len(header))
            if not chunk:
                return "ERROR|ConnectionClosed"
            header += chunk

        _, _, _, _, payload_len = struct.unpack('>4sBBH I', header[:12])

        payload = b''
        while len(payload) < payload_len:
            chunk = sock.recv(payload_len - len(payload))
            if not chunk:
                return "ERROR|ConnectionClosedDuringPayload"
            payload += chunk

        return payload.decode('utf-8', errors='replace')
    except socket.timeout:
        return "ERROR|Timeout"
    except Exception as e:
        return f"ERROR|{e}"
    finally:
        sock.close()


def wait_for_ipc(timeout: float = 30.0) -> bool:
    """Wait until yi3d IPC port is accepting connections."""
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(1.0)
            sock.connect((IPC_HOST, IPC_PORT))
            sock.close()
            return True
        except (ConnectionRefusedError, OSError, socket.timeout):
            time.sleep(1.0)
    return False


def is_ok(response: str) -> bool:
    return response.startswith('OK|')


def get_message(response: str) -> str:
    parts = response.split('|', 1)
    return parts[1] if len(parts) > 1 else response


def kill_yi3d():
    """Kill all YI3D.exe processes."""
    subprocess.run(['taskkill', '/f', '/im', 'YI3D.exe'],
                   capture_output=True, timeout=10)


# --- public API ---

def run_script(exe_path: str | Path, script_path: str | Path) -> tuple[bool, str]:
    """Execute a single script via YI3D.exe IPC.

    Launches YI3D.exe, connects, creates a new document,
    runs the script, then kills the process.

    Returns (passed, message).
    """
    exe_path = str(Path(exe_path).resolve())
    script_path = str(Path(script_path).resolve())
    start = time.time()

    # 1. Launch YI3D.exe
    try:
        proc = subprocess.Popen(
            [exe_path],
            cwd=os.path.dirname(exe_path),
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
    except FileNotFoundError:
        return (False, f"YI3D.exe not found: {exe_path}")

    # 2. Wait for IPC
    if not wait_for_ipc(timeout=30.0):
        kill_yi3d()
        return (False, "IPC not ready (timeout)")

    time.sleep(1.0)

    # 3. Create new document
    resp = ipc_request("command", "NewFile", timeout=30.0)
    if not is_ok(resp):
        kill_yi3d()
        return (False, f"NewFile failed: {get_message(resp)}")

    time.sleep(0.5)

    # 4. Run the script
    resp = ipc_request("script", script_path, timeout=120.0)
    elapsed = time.time() - start

    # 5. Kill yi3d
    kill_yi3d()
    time.sleep(0.5)

    if is_ok(resp):
        return (True, f"{elapsed:.1f}s")
    else:
        return (False, f"[{elapsed:.1f}s] {get_message(resp)}")


# --- CLI ---

def main():
    args = sys.argv[1:]
    exe_path = None
    script_path = None

    i = 0
    while i < len(args):
        if args[i] == "--exe" and i + 1 < len(args):
            exe_path = args[i + 1]
            i += 2
        elif args[i] == "--script" and i + 1 < len(args):
            script_path = args[i + 1]
            i += 2
        else:
            i += 1

    if not exe_path or not script_path:
        print("Usage: python run_script.py --exe <path> --script <script.py>")
        sys.exit(1)

    # Kill leftovers before starting
    kill_yi3d()
    time.sleep(1.0)

    passed, msg = run_script(exe_path, script_path)
    print(f"{'PASS' if passed else 'FAIL'} {msg}")
    sys.exit(0 if passed else 1)


if __name__ == "__main__":
    main()
