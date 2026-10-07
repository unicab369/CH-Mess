#!/usr/bin/env python3
"""Exercise this project's ATT client against Bumble's independent GATT server."""
import asyncio
import pathlib
import struct
import tempfile

from bumble import att, gatt_server
from bumble.core import UUID


ROOT = pathlib.Path(__file__).resolve().parents[2]
FIXTURE = pathlib.Path(__file__).with_name("ble_gatt_bumble_client_fixture.c")


class FakeDevice:
    def __init__(self):
        self.response = None
        self.l2cap_channel_manager = type(
            "FakeChannelManager", (), {"le_coc_channels": {}}
        )()

    def send_l2cap_pdu(self, connection_handle, cid, pdu):
        assert connection_handle == 1 and cid == att.ATT_CID
        self.response = bytes(pdu)


class FakeBearer:
    handle = 1
    att_mtu = 23
    encryption = False
    authenticated = False

    def on_att_mtu_update(self, mtu):
        self.att_mtu = mtu


class CAttClient:
    def __init__(self):
        self.tempdir = tempfile.TemporaryDirectory(prefix="ble-gatt-bumble-client-")
        self.binary = pathlib.Path(self.tempdir.name) / "att-client"
        import subprocess

        subprocess.run(
            ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
             str(FIXTURE), "-o", str(self.binary)],
            cwd=ROOT,
            check=True,
        )

    async def start(self):
        self.process = await asyncio.create_subprocess_exec(
            str(self.binary), stdin=asyncio.subprocess.PIPE,
            stdout=asyncio.subprocess.PIPE
        )

    async def transact(self, command, server, bearer, expected_status=0):
        self.process.stdin.write(bytes([command]))
        await self.process.stdin.drain()
        last_request = last_response = None
        while True:
            header = await asyncio.wait_for(self.process.stdout.readexactly(3), 2)
            kind = header[0]
            if kind == 2:
                status = header[1]
                length_high = await self.process.stdout.readexactly(1)
                result_len = header[2] | (length_high[0] << 8)
                assert status == expected_status, (
                    f"C client reported status {status}, expected "
                    f"{expected_status}; request={last_request!r}, "
                    f"response={last_response!r}"
                )
                result = await self.process.stdout.readexactly(result_len)
                assert last_response is not None and result == last_response
                return last_request, att.ATT_PDU.from_bytes(last_response)

            assert kind == 1
            request_len = struct.unpack_from("<H", header, 1)[0]
            assert request_len > 0
            request = await self.process.stdout.readexactly(request_len)
            parsed_request = att.ATT_PDU.from_bytes(request)

            device = server.device
            device.response = None
            server.on_gatt_pdu(bearer, parsed_request)
            for _ in range(20):
                await asyncio.sleep(0)
                if device.response is not None:
                    break
            assert device.response is not None, f"Bumble did not answer {parsed_request}"
            last_request, last_response = parsed_request, device.response
            framed = struct.pack("<H", len(device.response)) + device.response
            self.process.stdin.write(framed)
            await self.process.stdin.drain()

    async def write_command(self, server, bearer):
        self.process.stdin.write(b"\x0c")
        await self.process.stdin.drain()
        header = await asyncio.wait_for(self.process.stdout.readexactly(3), 2)
        assert header[0] == 1
        request_len = struct.unpack_from("<H", header, 1)[0]
        request = await self.process.stdout.readexactly(request_len)
        parsed_request = att.ATT_PDU.from_bytes(request)
        assert parsed_request.op_code == att.Opcode.ATT_WRITE_COMMAND

        device = server.device
        device.response = None
        server.on_gatt_pdu(bearer, parsed_request)
        for _ in range(20):
            await asyncio.sleep(0)
        assert device.response is None
        assert await asyncio.wait_for(self.process.stdout.readexactly(4), 2) == \
            bytes((2, 0, 0, 0))

    async def receive_server_event(self, command, server, bearer,
                                   characteristic, indication):
        self.process.stdin.write(bytes([command]))
        await self.process.stdin.drain()
        device = server.device
        device.response = None
        value = b"\x92" if indication else b"\x91"
        if indication:
            send_task = asyncio.create_task(server.indicate_subscriber(
                bearer, characteristic, value
            ))
            for _ in range(20):
                await asyncio.sleep(0)
                if device.response is not None:
                    break
        else:
            await server.notify_subscriber(bearer, characteristic, value)

        assert device.response is not None
        outgoing = bytes(device.response)
        expected_opcode = (att.Opcode.ATT_HANDLE_VALUE_INDICATION
                           if indication else
                           att.Opcode.ATT_HANDLE_VALUE_NOTIFICATION)
        assert att.ATT_PDU.from_bytes(outgoing).op_code == expected_opcode
        self.process.stdin.write(struct.pack("<H", len(outgoing)) + outgoing)
        await self.process.stdin.drain()

        event_seen = confirmation = None
        while event_seen is None or (indication and confirmation is None):
            kind = (await asyncio.wait_for(
                self.process.stdout.readexactly(1), 2
            ))[0]
            if kind == 3:
                header = await self.process.stdout.readexactly(4)
                handle, length = struct.unpack("<HH", header)
                event_value = await self.process.stdout.readexactly(length)
                assert handle == characteristic.handle and event_value == value
                event_seen = True
            elif kind == 1:
                header = await self.process.stdout.readexactly(2)
                length = struct.unpack("<H", header)[0]
                confirmation = await self.process.stdout.readexactly(length)
                expected_confirmation = bytes([
                    att.Opcode.ATT_HANDLE_VALUE_CONFIRMATION
                ])
                assert confirmation == expected_confirmation, (
                    f"{confirmation!r} != {expected_confirmation!r}"
                )
            else:
                raise AssertionError(f"unexpected fixture event {kind}")

        if indication:
            server.on_gatt_pdu(bearer, att.ATT_PDU.from_bytes(confirmation))
            await asyncio.wait_for(send_task, 2)
        else:
            try:
                await asyncio.wait_for(self.process.stdout.readexactly(1), 0.05)
            except asyncio.TimeoutError:
                pass
            else:
                raise AssertionError("notification unexpectedly sent a response")

    async def close(self):
        self.process.stdin.write(b"\x00")
        await self.process.stdin.drain()
        self.process.stdin.close()
        assert await asyncio.wait_for(self.process.wait(), 2) == 0
        self.tempdir.cleanup()


async def exercise():
    client = CAttClient()
    await client.start()
    device = FakeDevice()
    server = gatt_server.Server(device)
    descriptor = gatt_server.Descriptor(
        UUID(0x2901), att.Attribute.READABLE, b"sensor"
    )
    characteristic = gatt_server.Characteristic(
        UUID(0x2A19),
        gatt_server.Characteristic.Properties.READ |
        gatt_server.Characteristic.Properties.WRITE |
        gatt_server.Characteristic.Properties.NOTIFY |
        gatt_server.Characteristic.Properties.INDICATE,
        att.Attribute.READABLE | att.Attribute.WRITEABLE,
        bytes(range(70)),
        descriptors=[descriptor],
    )
    protected_characteristic = gatt_server.Characteristic(
        UUID(0x2A1A),
        gatt_server.Characteristic.Properties.READ,
        att.Attribute.READABLE | att.Attribute.READ_REQUIRES_ENCRYPTION,
        b"protected",
    )
    authenticated_characteristic = gatt_server.Characteristic(
        UUID(0x2A1B),
        gatt_server.Characteristic.Properties.READ,
        att.Attribute.READABLE | att.Attribute.READ_REQUIRES_AUTHENTICATION,
        b"authenticated",
    )
    service = gatt_server.Service(
        UUID(0x180F), [characteristic, protected_characteristic,
                       authenticated_characteristic]
    )
    included_service = gatt_server.Service(
        UUID(0x1812), [], included_services=[service]
    )
    server.add_service(service)
    server.add_service(included_service)
    bearer = FakeBearer()

    try:
        request, response = await client.transact(1, server, bearer)
        assert request.op_code == att.Opcode.ATT_EXCHANGE_MTU_REQUEST
        assert response.server_rx_mtu >= 64

        request, response = await client.transact(2, server, bearer)
        assert request.op_code == att.Opcode.ATT_READ_BY_GROUP_TYPE_REQUEST
        assert response.attributes[0][0] == service.handle

        request, response = await client.transact(3, server, bearer)
        assert request.op_code == att.Opcode.ATT_FIND_BY_TYPE_VALUE_REQUEST
        assert response.handles_information[0][0] == service.handle

        request, response = await client.transact(4, server, bearer)
        assert request.op_code == att.Opcode.ATT_READ_BY_TYPE_REQUEST
        assert response.attributes[0][0] == characteristic.handle - 1

        request, response = await client.transact(5, server, bearer)
        assert request.op_code == att.Opcode.ATT_FIND_INFORMATION_REQUEST
        assert response.information[0][0] == descriptor.handle

        request, response = await client.transact(19, server, bearer)
        assert request.op_code == att.Opcode.ATT_READ_BY_TYPE_REQUEST
        assert response.op_code == att.Opcode.ATT_READ_BY_TYPE_RESPONSE
        assert response.attributes == [(descriptor.handle, b"sensor")]

        request, response = await client.transact(20, server, bearer)
        assert request.op_code == att.Opcode.ATT_READ_BY_TYPE_REQUEST
        assert response.op_code == att.Opcode.ATT_READ_BY_TYPE_RESPONSE
        assert response.attributes == [(11, bytes((1, 0, 9, 0, 0x0F, 0x18)))]

        request, response = await client.transact(6, server, bearer)
        assert request.op_code == att.Opcode.ATT_READ_REQUEST
        assert response.attribute_value == bytes(range(63))

        request, response = await client.transact(18, server, bearer)
        assert request.op_code == att.Opcode.ATT_READ_MULTIPLE_REQUEST
        assert response.op_code == att.Opcode.ATT_READ_MULTIPLE_RESPONSE
        assert response.set_of_values == bytes(range(63))

        assert protected_characteristic.handle == 7
        request, response = await client.transact(
            16, server, bearer, expected_status=3
        )
        assert request.op_code == att.Opcode.ATT_READ_REQUEST
        assert response.op_code == att.Opcode.ATT_ERROR_RESPONSE
        assert response.request_opcode_in_error == att.Opcode.ATT_READ_REQUEST
        assert response.attribute_handle_in_error == protected_characteristic.handle
        assert response.error_code == 0x0F  # Insufficient Encryption

        request, response = await client.transact(
            17, server, bearer, expected_status=3
        )
        assert request.op_code == att.Opcode.ATT_READ_REQUEST
        assert response.op_code == att.Opcode.ATT_ERROR_RESPONSE
        assert response.request_opcode_in_error == att.Opcode.ATT_READ_REQUEST
        assert response.attribute_handle_in_error == \
            authenticated_characteristic.handle
        assert response.error_code == 0x05  # Insufficient Authentication

        request, response = await client.transact(10, server, bearer)
        assert request.op_code == att.Opcode.ATT_READ_BLOB_REQUEST
        assert response.part_attribute_value == bytes(range(63, 70))

        request, response = await client.transact(7, server, bearer)
        assert request.op_code == att.Opcode.ATT_WRITE_REQUEST
        assert response.op_code == att.Opcode.ATT_WRITE_RESPONSE

        request, response = await client.transact(8, server, bearer)
        assert request.op_code == att.Opcode.ATT_PREPARE_WRITE_REQUEST
        assert response.part_attribute_value == b"\x55"

        request, response = await client.transact(9, server, bearer)
        assert request.op_code == att.Opcode.ATT_EXECUTE_WRITE_REQUEST
        assert response.op_code == att.Opcode.ATT_EXECUTE_WRITE_RESPONSE
        assert characteristic.value == b"\x55"

        request, response = await client.transact(11, server, bearer)
        assert request.op_code == att.Opcode.ATT_EXECUTE_WRITE_REQUEST
        assert response.op_code == att.Opcode.ATT_EXECUTE_WRITE_RESPONSE
        assert characteristic.value == bytes((0x80 + i) & 0xFF for i in range(70))

        request, response = await client.transact(13, server, bearer)
        assert request.op_code == att.Opcode.ATT_WRITE_REQUEST
        assert response.op_code == att.Opcode.ATT_WRITE_RESPONSE
        assert server.subscribers[bearer][characteristic.handle] == b"\x03\x00"
        await client.receive_server_event(14, server, bearer, characteristic,
                                          indication=False)
        await client.receive_server_event(15, server, bearer, characteristic,
                                          indication=True)

        await client.write_command(server, bearer)
        assert characteristic.value == b"\x44"
    finally:
        await client.close()


def main():
    asyncio.run(exercise())
    print("C GATT client / Bumble GATT server interoperability: PASS")


if __name__ == "__main__":
    main()
