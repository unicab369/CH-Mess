#!/usr/bin/env python3
"""Check the C LE L2CAP/SMP bearer against Bumble's independent SMP codec."""
import pathlib
import struct
import subprocess
import tempfile

from bumble import smp


ROOT = pathlib.Path(__file__).resolve().parents[2]
FIXTURE = ROOT / "tests" / "ble_smp_bumble_fixture.c"


def read_record(stream):
    header = stream.read(2)
    if len(header) != 2:
        raise RuntimeError("C SMP fixture closed its output")
    length = struct.unpack("<H", header)[0]
    data = stream.read(length)
    if len(data) != length:
        raise RuntimeError("truncated record from C SMP fixture")
    return data


def main():
    with tempfile.TemporaryDirectory(prefix="ble-smp-bumble-") as temp:
        binary = pathlib.Path(temp) / "smp-fixture"
        subprocess.run(
            ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
             str(FIXTURE), "-o", str(binary)],
            cwd=ROOT,
            check=True,
        )
        process = subprocess.Popen(
            [str(binary)], stdin=subprocess.PIPE, stdout=subprocess.PIPE
        )
        try:
            l2cap_sdu = read_record(process.stdout)
            assert len(l2cap_sdu) >= 4
            sdu_length, cid = struct.unpack_from("<HH", l2cap_sdu)
            assert cid == smp.SMP_CID
            assert sdu_length == len(l2cap_sdu) - 4
            request = smp.SMP_Command.from_bytes(l2cap_sdu[4:])
            assert isinstance(request, smp.SMP_Pairing_Request_Command)
            assert request.io_capability == smp.IoCapability.NO_INPUT_NO_OUTPUT
            assert request.maximum_encryption_key_size == 16
            assert int(request.auth_req) == 0x09

            response = smp.SMP_Pairing_Response_Command(
                io_capability=smp.IoCapability.NO_INPUT_NO_OUTPUT,
                oob_data_flag=0,
                auth_req=smp.AuthReq.from_booleans(bonding=True, sc=True),
                maximum_encryption_key_size=16,
                initiator_key_distribution=smp.KeyDistribution(3),
                responder_key_distribution=smp.KeyDistribution(3),
            )
            response_pdu = bytes(response)
            l2cap_frame = struct.pack("<HH", len(response_pdu), cid) + response_pdu
            process.stdin.write(struct.pack("<H", len(l2cap_frame)) + l2cap_frame)
            process.stdin.flush()
            returned_pdu = read_record(process.stdout)
            assert returned_pdu == response_pdu
            process.stdin.close()
            assert process.wait(timeout=2) == 0
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()
    print("C L2CAP/SMP bearer ↔ Bumble SMP codec interoperability: PASS")


if __name__ == "__main__":
    main()
