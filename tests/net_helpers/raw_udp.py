"""RAW UDP send / receive helper.

Wraps a SOCK_DGRAM socket with a small, test-friendly API:

  - bind to an ephemeral or specified local port,
  - send to any peer (`SO_BROADCAST` is enabled by default so 255.255.255.255
    works without extra configuration),
  - receive one datagram with a per-call timeout.

Sufficient for "Python ↔ DUT byte exchange" tests; pair with the
VBAN / RTP helpers when wire framing is needed on top.
"""

from __future__ import annotations

import socket
import time
from typing import Optional, Tuple


class RawUdp:
    def __init__(self, bind_port: int = 0, *, bind_host: str = "0.0.0.0"):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        self.sock.bind((bind_host, bind_port))
        # Default to non-blocking; recv() sets its own per-call timeout.
        self.sock.setblocking(False)

    @property
    def local_port(self) -> int:
        return self.sock.getsockname()[1]

    def send(self, data: bytes, host: str, port: int) -> int:
        return self.sock.sendto(data, (host, port))

    def recv(self, max_bytes: int = 2048,
             timeout: float = 2.0) -> Optional[Tuple[bytes, Tuple[str, int]]]:
        """Wait up to `timeout` seconds for one datagram.

        Returns (data, (host, port)) or None on timeout.
        """
        deadline = time.monotonic() + timeout
        while True:
            try:
                data, addr = self.sock.recvfrom(max_bytes)
                return data, addr
            except BlockingIOError:
                if time.monotonic() >= deadline:
                    return None
                time.sleep(0.01)

    def close(self) -> None:
        self.sock.close()

    def __enter__(self) -> "RawUdp":
        return self

    def __exit__(self, *exc) -> None:
        self.close()
