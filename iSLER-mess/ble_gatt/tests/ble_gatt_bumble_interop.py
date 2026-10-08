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
from cryptography.hazmat.primitives.ciphers import algorithms
from cryptography.hazmat.primitives.cmac import CMAC


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
        response = read_exact(self.process.stdout, length) if length else b""
        event_length = struct.unpack("<H", read_exact(self.process.stdout, 2))[0]
        event = read_exact(self.process.stdout, event_length) if event_length else b""
        return response, event

    def set_link_security(self, encrypted, key_size, authorized):
        response, event = self.exchange(bytes((
            0xF0, int(encrypted), key_size, int(authorized)
        )))
        assert response == b"" and event == b""

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
        response, event = self.server.exchange(pdu)
        for received in (response, event):
            if received:
                self.client.on_gatt_pdu(att.ATT_PDU.from_bytes(received))

    def on_att_mtu_update(self, mtu):
        self.att_mtu = mtu


async def exercise(server):
    bearer = FixtureBearer(server)
    client = Client(bearer)
    bearer.client = client

    assert await client.request_mtu(64) == 64
    services = await client.discover_services()
    assert [service.uuid for service in services] == [
        UUID(0x180F), UUID(0x1812), UUID(0x1801), UUID(0x1800)
    ]

    battery_service = services[0]
    find_battery = await client.send_request(
        att.ATT_Find_By_Type_Value_Request(
            starting_handle=1,
            ending_handle=0xFFFF,
            attribute_type=UUID(0x2800),
            attribute_value=bytes((0x0F, 0x18)),
        )
    )
    assert find_battery.handles_information == [
        (battery_service.handle, battery_service.end_group_handle)
    ]
    by_uuid = await client.discover_service(UUID(0x180F))
    assert len(by_uuid) == 1 and by_uuid[0].handle == battery_service.handle

    included = await client.discover_included_services(services[1])
    assert len(included) == 1
    assert included[0].handle == battery_service.handle
    assert included[0].end_group_handle == battery_service.end_group_handle
    assert included[0].uuid == UUID(0x180F)

    database_hash_values = await client.read_characteristics_by_uuid(
        UUID(0x2B2A), None
    )
    hash_input = bytes.fromhex(
        "010000280f18 020003288a0300192a 040000290200 05000129 "
        "060000281218 07000228010005000f18 0b0000280118 "
        "0c000328200d00052a 0e000229 0f0003280210002a2b "
        "110003280a1200292b 130000280018 "
        "14000328021500002a 16000328021700012a "
        "18000328021900042a 1a000328021b00a62a "
        "1c000328021d00f52b 1e000328021f00882b "
        "20000328422100f0ff"
    )
    cmac = CMAC(algorithms.AES(bytes(16)))
    cmac.update(hash_input)
    assert database_hash_values == [cmac.finalize()]

    gap_service = services[3]
    gap_characteristics = await client.discover_characteristics([], gap_service)
    gap_values = {}
    edkm_characteristic = None
    signed_characteristic = None
    for characteristic in gap_characteristics:
        if characteristic.uuid == UUID(0x2B88):
            edkm_characteristic = characteristic
            continue
        if characteristic.uuid == UUID(0xFFF0):
            signed_characteristic = characteristic
            continue
        gap_values[characteristic.uuid] = await characteristic.read_value()
    assert gap_values == {
        UUID(0x2A00): b"CH-Mess",
        UUID(0x2A01): b"\x00\x00",
        UUID(0x2A04): b"\xff" * 8,
        UUID(0x2AA6): b"\x01",
        UUID(0x2BF5): b"\x01\x03",
    }
    assert edkm_characteristic is not None
    assert signed_characteristic is not None
    try:
        await edkm_characteristic.read_value()
    except att.ATT_Error as error:
        assert error.error_code == att.ATT_INSUFFICIENT_AUTHENTICATION_ERROR
    else:
        raise AssertionError("Encrypted Data Key Material was readable in clear")

    gatt_service = services[2]
    gatt_characteristics = await client.discover_characteristics([], gatt_service)
    changed = next(c for c in gatt_characteristics if c.uuid == UUID(0x2A05))
    client_features = next(c for c in gatt_characteristics
                           if c.uuid == UUID(0x2B29))
    assert await client_features.read_value() == b"\x00"
    await client_features.write_value(b"\x04", with_response=True)
    assert await client_features.read_value() == b"\x04"
    try:
        await client_features.write_value(b"\x00", with_response=True)
    except att.ATT_Error as error:
        assert error.error_code == 0x13  # Value Not Allowed.
    else:
        raise AssertionError("Client Supported Features bits were cleared")
    changed_events = []
    await changed.subscribe(lambda value: changed_events.append(value),
                            prefer_notify=False)
    assert changed_events == [b"\x01\x00\xff\xff"]

    characteristics = await client.discover_characteristics([], battery_service)
    assert len(characteristics) == 1
    characteristic = characteristics[0]
    assert characteristic.uuid == UUID(0x2A19)

    descriptors = await client.discover_descriptors(characteristic)
    assert [descriptor.type for descriptor in descriptors] == [
        UUID(0x2900), UUID(0x2901)
    ]
    user_description = descriptors[1]
    assert await user_description.read_value() == b"x"
    await user_description.write_value(b"User label", with_response=True)
    assert await user_description.read_value() == b"User label"

    original = await characteristic.read_value()
    assert original == bytes(range(70))

    fixed_multiple = await client.send_request(att.ATT_Read_Multiple_Request(
        set_of_handles=[characteristic.handle, descriptors[1].handle]
    ))
    assert fixed_multiple.set_of_values == bytes(range(63))

    multiple = await client.send_request(att.ATT_Read_Multiple_Variable_Request(
        set_of_handles=[characteristic.handle, descriptors[1].handle]
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
        await client.read_value(8)
    except att.ATT_Error as error:
        assert error.error_code == att.ATT_INSUFFICIENT_ENCRYPTION_ERROR
        assert error.message.request_opcode_in_error == att.Opcode.ATT_READ_REQUEST
        assert error.message.attribute_handle_in_error == 8
    else:
        raise AssertionError("protected C attribute was readable without encryption")

    try:
        await client.read_value(9)
    except att.ATT_Error as error:
        assert error.error_code == att.ATT_INSUFFICIENT_AUTHENTICATION_ERROR
        assert error.message.request_opcode_in_error == att.Opcode.ATT_READ_REQUEST
        assert error.message.attribute_handle_in_error == 9
    else:
        raise AssertionError(
            "authenticated C attribute was readable without authentication"
        )

    try:
        await client.read_value(10)
    except att.ATT_Error as error:
        assert error.error_code == att.ATT_INSUFFICIENT_AUTHORIZATION_ERROR
        assert error.message.request_opcode_in_error == att.Opcode.ATT_READ_REQUEST
        assert error.message.attribute_handle_in_error == 10
    else:
        raise AssertionError("application-protected C attribute was readable")

    # The test harness changes link security out of band, then confirms the
    # server reports the ATT key-size error after authorization succeeds.
    server.set_link_security(encrypted=True, key_size=7, authorized=True)
    try:
        await client.read_value(10)
    except att.ATT_Error as error:
        assert error.error_code == 0x0C
    else:
        raise AssertionError("short encryption key was accepted")

    server.set_link_security(encrypted=False, key_size=0, authorized=True)
    signed_header = bytes((0xD2, signed_characteristic.handle & 0xff,
                           signed_characteristic.handle >> 8, 0x5A))
    counter = b"\x01\x00\x00\x00"
    signed_cmac = CMAC(algorithms.AES(bytes.fromhex(
        "611b64ebfbcd1fd372ec9196df425e50")))
    signed_cmac.update(signed_header + counter)
    signed_pdu = signed_header + counter + signed_cmac.finalize()[:8][::-1]
    response, event = server.exchange(signed_pdu)
    assert response == b"" and event == b""
    assert await client.read_value(signed_characteristic.handle) == b"\x5a"
    # A repeated signature counter is ignored and cannot rewrite the value.
    server.exchange(signed_pdu)
    assert await client.read_value(signed_characteristic.handle) == b"\x5a"


def main():
    server = CAttServer()
    try:
        asyncio.run(exercise(server))
    finally:
        server.close()
    print("Bumble GATT client interoperability: PASS")


if __name__ == "__main__":
    main()
