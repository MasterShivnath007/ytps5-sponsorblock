#!/usr/bin/env python3
"""
ytps5-sponsorblock helper.

Examples:
  python3 tools/ytps5.py send --host 192.168.1.50
  python3 tools/ytps5.py logs --host 192.168.1.50
  python3 tools/ytps5.py upload --host 192.168.1.50
"""

from __future__ import annotations

import argparse
import ftplib
import io
import pathlib
import socket
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parent.parent
REMOTE_DIR = "/data/ytps5-sponsorblock"
FTP_ERRORS = ftplib.all_errors + (OSError,)
DEFAULT_ELF = ROOT / "build" / "ytps5-injector.elf"
DEFAULT_SO = ROOT / "build" / "sponsorblock.so"
DEFAULT_JS = ROOT / "payload" / "inject.js"

LOG_FILES = [
    f"{REMOTE_DIR}/injector.log",
    f"{REMOTE_DIR}/ytps5.log",
    "/download0/ytps5.log",
    "/mnt/sandbox/PPSA01650_000/download0/ytps5.log",
    "/mnt/sandbox/PPSA01651_000/download0/ytps5.log",
    "/mnt/sandbox/PPSA01652_000/download0/ytps5.log",
    "/mnt/sandbox/PPSA01650_000/download0/hui/dev/yt.log",
    "/mnt/sandbox/PPSA01651_000/download0/hui/dev/yt.log",
    "/mnt/sandbox/PPSA01652_000/download0/hui/dev/yt.log",
]


def require(path: pathlib.Path, label: str) -> pathlib.Path:
    if not path.exists():
        sys.exit(
            f"missing {label}: {path}\n"
            "Build it first with `make` (see README.md)."
        )
    return path


def ftp_connect(host: str, port: int) -> ftplib.FTP:
    ftp = ftplib.FTP()
    ftp.connect(host, port, timeout=15)
    ftp.login("anonymous", "anonymous")
    ftp.set_pasv(True)
    return ftp


def ensure_remote_dir(ftp: ftplib.FTP, path: str) -> None:
    parts = [p for p in path.split("/") if p]
    cur = ""
    for part in parts:
        cur += "/" + part
        try:
            ftp.mkd(cur)
        except ftplib.error_perm:
            pass


def upload_file(ftp: ftplib.FTP, local: pathlib.Path, remote: str) -> None:
    with local.open("rb") as fh:
        ftp.storbinary(f"STOR {remote}", fh)
    print(f"uploaded {local.name} -> {remote} ({local.stat().st_size} bytes)")


def send_elf(host: str, port: int, elf: pathlib.Path) -> None:
    data = elf.read_bytes()
    print(f"sending {elf.name} ({len(data)} bytes) to {host}:{port} ...")
    with socket.create_connection((host, port), timeout=15) as sock:
        sock.sendall(data)
        try:
            sock.shutdown(socket.SHUT_WR)
        except OSError:
            pass
    print("ELF sent")


def cmd_upload(args: argparse.Namespace) -> None:
    elf = require(pathlib.Path(args.elf), "injector ELF")
    so = require(pathlib.Path(args.so), "module")
    js = require(pathlib.Path(args.js), "JS payload")

    ftp = ftp_connect(args.host, args.ftp_port)
    try:
        ensure_remote_dir(ftp, REMOTE_DIR)
        upload_file(ftp, so, f"{REMOTE_DIR}/sponsorblock.so")
        upload_file(ftp, js, f"{REMOTE_DIR}/inject.js")
        upload_file(ftp, elf, f"{REMOTE_DIR}/ytps5-injector.elf")
    finally:
        ftp.quit()
    print("upload complete")


def cmd_send(args: argparse.Namespace) -> None:
    cmd_upload(args)
    if args.no_send:
        return

    elf = pathlib.Path(args.elf)
    send_elf(args.host, args.elf_port, elf)

    if args.wait:
        print(f"waiting {args.wait}s for the app to load the module ...")
        time.sleep(args.wait)
        cmd_logs(args)


def download_text(ftp: ftplib.FTP, remote: str) -> str | None:
    buf = io.BytesIO()
    try:
        ftp.retrbinary(f"RETR {remote}", buf.write)
    except ftplib.error_perm as exc:
        if str(exc).startswith("550"):
            return None
        raise
    return buf.getvalue().decode("utf-8", "replace")


def cmd_logs(args: argparse.Namespace) -> None:
    ftp = ftp_connect(args.host, args.ftp_port)
    found = 0
    out_dir = ROOT / "logs"
    if args.save:
        out_dir.mkdir(exist_ok=True)

    try:
        for remote in LOG_FILES:
            try:
                text = download_text(ftp, remote)
            except FTP_ERRORS as exc:
                print(f"[could not read {remote}: {exc}]", file=sys.stderr)
                continue
            if text is None:
                continue
            found += 1
            print("=" * 78)
            print(remote)
            print("=" * 78)
            print(text.rstrip())
            print()
            if args.save:
                name = remote.replace("/", "_").lstrip("_")
                (out_dir / name).write_text(text, encoding="utf-8")
    finally:
        ftp.quit()

    if not found:
        sys.exit(
            "no logs found. Check that:\n"
            "  - etaHEN FTP is running on port 2121\n"
            "  - the injector ran at least once\n"
            "  - YouTube has been started after the injector was sent"
        )


def cmd_clean_logs(args: argparse.Namespace) -> None:
    ftp = ftp_connect(args.host, args.ftp_port)
    try:
        for remote in LOG_FILES:
            try:
                ftp.delete(remote)
                print(f"deleted {remote}")
            except ftplib.error_perm:
                pass
    finally:
        ftp.quit()


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__)
    sub = p.add_subparsers(dest="command", required=True)

    def common(sp: argparse.ArgumentParser) -> None:
        sp.add_argument("--host", required=True, help="PS5 IP address")
        sp.add_argument("--ftp-port", type=int, default=2121)
        sp.add_argument("--elf", default=str(DEFAULT_ELF))
        sp.add_argument("--so", default=str(DEFAULT_SO))
        sp.add_argument("--js", default=str(DEFAULT_JS))

    up = sub.add_parser("upload", help="upload module + JS + ELF over FTP")
    common(up)
    up.set_defaults(func=cmd_upload)

    send = sub.add_parser("send", help="upload then send the ELF to elfldr")
    common(send)
    send.add_argument("--elf-port", type=int, default=9021)
    send.add_argument("--no-send", action="store_true",
                      help="only upload, do not send the ELF")
    send.add_argument("--wait", type=int, default=0,
                      help="seconds to wait before printing logs")
    send.add_argument("--save", action="store_true",
                      help="also save logs under ./logs/ when --wait is used")
    send.set_defaults(func=cmd_send)

    logs = sub.add_parser("logs", help="download logs over FTP")
    common(logs)
    logs.add_argument("--save", action="store_true",
                      help="also save logs under ./logs/")
    logs.set_defaults(func=cmd_logs)

    clean = sub.add_parser("clean-logs", help="delete remote log files")
    common(clean)
    clean.set_defaults(func=cmd_clean_logs)

    return p


def main() -> None:
    args = build_parser().parse_args()
    try:
        args.func(args)
    except FTP_ERRORS as exc:
        sys.exit(
            f"connection failed: {exc}\n"
            "Check the PS5 IP, that etaHEN FTP is running on port 2121, "
            "and that elfldr is listening on port 9021."
        )


if __name__ == "__main__":
    main()
