#!/usr/bin/env python3
"""Run Bumble's independent GATT client against the C ATT server fixture."""
import asyncio
import pathlib
import struct
import subprocess
import tempfile

from bumble import att, utils
from bumble.core import UUID
from bumble.gatt_client import Client


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
        self.tempdir = tempfile.TemporaryDirectory(prefix="ble-gatt-bumble-")
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
        return read_exact(self.process.stdout, length) if length else b""

    def close(self):
        self.process.stdin.close()
        if self.process.wait(timeout=2) != 0:
            raise RuntimeError("C ATT fixture exited with an error")
        self.tempdir.cleanup()


class FixtureBearer(utils.EventEmitter):
    EVENT_DISCONNECTION = "disconnection"

    def __init__(self, server):
        super().__init__()
        self.server = server
        self.handle = 1
        self.att_mtu = att.ATT_DEFAULT_MTU
        self.client = None

    def send_l2cap_pdu(self, cid, pdu):
        assert cid == att.ATT_CID
        response = self.server.exchange(pdu)
        if response:
            self.client.on_gatt_pdu(att.ATT_PDU.from_bytes(response))

    def on_att_mtu_update(self, mtu):
        self.att_mtu = mtu


async def exercise(server):
    bearer = FixtureBearer(server)
    client = Client(bearer)
    bearer.client = client

    assert await client.request_mtu(64) == 64
    services = await client.discover_services()
    assert [service.uuid for service in services] == [UUID(0x180F), UUID(0x1812)]

    battery_service = services[0]
    by_uuid = await client.discover_service(UUID(0x180F))
    assert len(by_uuid) == 1 and by_uuid[0].handle == battery_service.handle

    included = await client.discover_included_services(services[1])
    assert len(included) == 1
    assert included[0].handle == battery_service.handle
    assert included[0].end_group_handle == battery_service.end_group_handle
    assert included[0].uuid == UUID(0x180F)

    characteristics = await client.discover_characteristics([], battery_service)
    assert len(characteristics) == 1
    characteristic = characteristics[0]
    assert characteristic.uuid == UUID(0x2A19)

    descriptors = await client.discover_descriptors(characteristic)
    assert [descriptor.type for descriptor in descriptors] == [UUID(0x2901)]

    original = await characteristic.read_value()
    assert original == bytes(range(70))

    fixed_multiple = await client.send_request(att.ATT_Read_Multiple_Request(
        set_of_handles=[characteristic.handle, descriptors[0].handle]
    ))
    assert fixed_multiple.set_of_values == bytes(range(63))

    multiple = await client.send_request(att.ATT_Read_Multiple_Variable_Request(
        set_of_handles=[characteristic.handle, descriptors[0].handle]
    ))
    assert multiple.length_value_tuple_list == [(70, bytes(range(61)))]

    replacement = bytes((0xA0 + i) & 0xFF for i in range(80))
    await characteristic.write_value(replacement, with_response=True)
    assert await characteristic.read_value() == replacement

    end_read = await client.send_request(att.ATT_Read_Blob_Request(
        attribute_handle=characteristic.handle,
        value_offset=len(replacement),
    ))
    assert end_read.part_attribute_value == b""
    past_end = await client.send_request(att.ATT_Read_Blob_Request(
        attribute_handle=characteristic.handle,
        value_offset=len(replacement) + 1,
    ))
    assert past_end.op_code == att.Opcode.ATT_ERROR_RESPONSE
    assert past_end.error_code == 0x07  # Invalid Offset
    assert past_end.request_opcode_in_error == att.Opcode.ATT_READ_BLOB_REQUEST
    assert past_end.attribute_handle_in_error == characteristic.handle

    try:
        await client.read_value(7)
    except att.ATT_Error as error:
        assert error.error_code == att.ATT_INSUFFICIENT_ENCRYPTION_ERROR
        assert error.message.request_opcode_in_error == att.Opcode.ATT_READ_REQUEST
        assert error.message.attribute_handle_in_error == 7
    else:
        raise AssertionError("protected C attribute was readable without encryption")

    try:
        await client.read_value(8)
    except att.ATT_Error as error:
        assert error.error_code == att.ATT_INSUFFICIENT_AUTHENTICATION_ERROR
        assert error.message.request_opcode_in_error == att.Opcode.ATT_READ_REQUEST
        assert error.message.attribute_handle_in_error == 8
    else:
        raise AssertionError(
            "authenticated C attribute was readable without authentication"
        )

    try:
        await client.read_value(9)
    except att.ATT_Error as error:
        assert error.error_code == att.ATT_INSUFFICIENT_AUTHORIZATION_ERROR
        assert error.message.request_opcode_in_error == att.Opcode.ATT_READ_REQUEST
        assert error.message.attribute_handle_in_error == 9
    else:
        raise AssertionError("application-protected C attribute was readable")


def main():
    server = CAttServer()
    try:
        asyncio.run(exercise(server))
    finally:
        server.close()
    print("Bumble GATT client interoperability: PASS")


if __name__ == "__main__":
    main()
