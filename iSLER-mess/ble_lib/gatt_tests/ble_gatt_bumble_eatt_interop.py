#!/usr/bin/env python3
"""Exercise Bumble's EATT client bearer against the C GATT server fixture."""
import asyncio
import pathlib
import struct
import subprocess
import tempfile
from types import SimpleNamespace

from bumble import att
from bumble.gatt_client import Client
from bumble.l2cap import LeCreditBasedChannel
from bumble.core import UUID


ROOT = pathlib.Path(__file__).resolve().parents[2]
FIXTURE = pathlib.Path(__file__).with_name("ble_gatt_bumble_fixture.c")


def read_exact(stream, count):
    data = bytearray()
    while len(data) < count:
        block = stream.read(count - len(data))
        if not block:
            raise RuntimeError("C ATT fixture closed its output")
        data.extend(block)
    return bytes(data)


class CAttServer:
    def __init__(self):
        self.tempdir = tempfile.TemporaryDirectory(prefix="ble-gatt-eatt-")
        self.binary = pathlib.Path(self.tempdir.name) / "att-server"
        subprocess.run(
            ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
             str(FIXTURE), "-o", str(self.binary)],
            cwd=ROOT,
            check=True,
        )
        self.process = subprocess.Popen(
            [str(self.binary)], stdin=subprocess.PIPE, stdout=subprocess.PIPE
        )

    def exchange(self, pdu):
        self.process.stdin.write(struct.pack("<H", len(pdu)) + pdu)
        self.process.stdin.flush()
        length = struct.unpack("<H", read_exact(self.process.stdout, 2))[0]
        response = read_exact(self.process.stdout, length)
        event_length = struct.unpack("<H", read_exact(self.process.stdout, 2))[0]
        event = read_exact(self.process.stdout, event_length)
        return response, event

    def close(self):
        self.process.stdin.close()
        if self.process.wait(timeout=2) != 0:
            raise RuntimeError("C ATT fixture exited with an error")
        self.tempdir.cleanup()


async def exercise(server):
    channel = LeCreditBasedChannel(
        None, SimpleNamespace(handle=1), att.EATT_PSM,
        0x0040, 0x0041, 100, 100, 8, 100, 100, 8, True,
    )
    client = Client(channel)
    channel.sink = lambda pdu: client.on_gatt_pdu(att.ATT_PDU.from_bytes(pdu))

    def write(pdu):
        response, event = server.exchange(pdu)
        if response:
            channel.sink(response)
        if event:
            channel.sink(event)

    channel.write = write
    assert await client.request_mtu(80) == 64
    services = await client.discover_services()
    battery = next(service for service in services if service.uuid == UUID(0x180F))
    characteristics = await client.discover_characteristics([], battery)
    assert len(characteristics) == 1
    value = await characteristics[0].read_value()
    assert value == bytes(range(70))


def main():
    server = CAttServer()
    try:
        asyncio.run(exercise(server))
    finally:
        server.close()
    print("Bumble EATT bearer interoperability: PASS")


if __name__ == "__main__":
    main()
