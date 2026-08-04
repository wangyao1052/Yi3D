import argparse
import datetime
import json
import socket

HOST = "127.0.0.1"
PORT = 17999
RESPONSE_LOG_FILE = "python_client_response.log"
DEFAULT_TIMEOUT = 5.0
SCRIPT_TIMEOUT = 60.0
COMMAND_TIMEOUT = 10.0

# IPC frame format (big-endian):
#   0-3   magic       4B  b"YI3D"
#   4     version     1B  0x01
#   5     msg_type    1B  0x01=request, 0x02=response
#   6-7   reserved    2B  must be 0
#   8-11  payload_len 4B  N (1..16MB)
#   12..  payload     NB  UTF-8 JSON
MAGIC = b"YI3D"
VERSION = 0x01
MSG_TYPE_REQUEST = 0x01
MSG_TYPE_RESPONSE = 0x02
HEADER_SIZE = 12
MAX_PAYLOAD_BYTES = 16 * 1024 * 1024


def _parse_response(line):
    if not line:
        return {"ok": False, "error": "empty response"}

    if "|" in line:
        prefix, msg = line.split("|", 1)
    else:
        return {"ok": False, "error": f"unexpected response: {line}"}

    prefix = prefix.strip().upper()
    msg = msg.strip().replace("\\r", "\r").replace("\\n", "\n")
    if prefix == "OK":
        return {"ok": True, "data": msg}
    return {"ok": False, "error": msg or "unknown error"}


def _write_response_log(method, argument, raw_line, parsed):
    timestamp = datetime.datetime.now().isoformat(timespec="seconds")
    record = {
        "time": timestamp,
        "request_method": method,
        "request_argument": argument,
        "raw_response": raw_line,
        "parsed": parsed,
    }
    with open(RESPONSE_LOG_FILE, "a", encoding="utf-8") as f:
        f.write(json.dumps(record, ensure_ascii=False) + "\n")


def _pack_frame(msg_type, payload):
    if not isinstance(payload, (bytes, bytearray)):
        raise TypeError("payload must be bytes")
    payload_len = len(payload)
    if payload_len <= 0 or payload_len > MAX_PAYLOAD_BYTES:
        raise ValueError(f"payload length out of range: {payload_len}")

    header = bytearray()
    header.extend(MAGIC)
    header.append(VERSION)
    header.append(msg_type)
    header.extend(b"\x00\x00")  # reserved
    header.extend(payload_len.to_bytes(4, byteorder="big", signed=False))
    return bytes(header) + payload


def _recv_exact(sock, n):
    chunks = []
    remaining = n
    while remaining > 0:
        chunk = sock.recv(remaining)
        if not chunk:
            raise RuntimeError("socket closed before receiving full frame")
        chunks.append(chunk)
        remaining -= len(chunk)
    return b"".join(chunks)


def call(method, argument, timeout=5, host=HOST, port=PORT):
    request = {
        "commnad": method.strip(),
        "argument": argument.strip(),
    }
    payload = json.dumps(request, ensure_ascii=False).encode("utf-8")
    frame = _pack_frame(MSG_TYPE_REQUEST, payload)
    with socket.create_connection((host, port), timeout=timeout) as s:
        s.sendall(frame)
        header = _recv_exact(s, HEADER_SIZE)
        if header[:4] != MAGIC:
            raise RuntimeError(f"invalid magic: {header[:4]!r}")
        if header[4] != VERSION:
            raise RuntimeError(f"unsupported version: {header[4]}")
        if header[5] != MSG_TYPE_RESPONSE:
            raise RuntimeError(f"invalid message type: {header[5]}")
        if header[6] != 0 or header[7] != 0:
            raise RuntimeError("invalid reserved field")

        payload_len = int.from_bytes(header[8:12], byteorder="big", signed=False)
        if payload_len <= 0 or payload_len > MAX_PAYLOAD_BYTES:
            raise RuntimeError(f"invalid payload_len: {payload_len}")
        data = _recv_exact(s, payload_len)

    line = data.decode("utf-8", errors="replace")
    parsed = _parse_response(line)
    _write_response_log(method, argument, line, parsed)
    return parsed


def run_file(path, timeout=30):
    return call("script", path, timeout=timeout)


def parse_args():
    parser = argparse.ArgumentParser(description="YI3D IPC client")
    parser.add_argument("--method", required=True, help="IPC method, e.g. script/ping/command")
    parser.add_argument("--argument", default="", help="IPC argument")
    parser.add_argument("--timeout", type=float, default=None, help="Socket timeout in seconds")
    parser.add_argument("--host", default=HOST, help="IPC server host")
    parser.add_argument("--port", type=int, default=PORT, help="IPC server port")
    return parser.parse_args()


if __name__ == "__main__":
    args = parse_args()
    timeout = args.timeout
    if timeout is None:
        method_lower = args.method.strip().lower()
    if method_lower == "command":
        timeout = COMMAND_TIMEOUT
    elif method_lower == "script":
        timeout = SCRIPT_TIMEOUT
    else:
        timeout = DEFAULT_TIMEOUT
    print(call(args.method, args.argument, timeout=timeout, host=args.host, port=args.port))
