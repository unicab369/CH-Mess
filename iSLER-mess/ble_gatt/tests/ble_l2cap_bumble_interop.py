#!/usr/bin/env python3
"""Exercise LE L2CAP CoC setup, data, and credit exchange with Bumble."""
import pathlib
import struct
import subprocess
import tempfile

from bumble import l2cap


ROOT = pathlib.Path(__file__).resolve().parents[2]
FIXTURE = ROOT / "tests" / "ble_l2cap_bumble_fixture.c"


def read_record(stream):
    header = stream.read(2)
    if len(header) != 2:
        raise RuntimeError("C L2CAP fixture closed its output")
    length = struct.unpack("<H", header)[0]
    data = stream.read(length)
    if len(data) != length:
        raise RuntimeError("truncated record from C L2CAP fixture")
    return data


def write_record(stream, data):
    stream.write(struct.pack("<H", len(data)) + data)
    stream.flush()


def main():
    with tempfile.TemporaryDirectory(prefix="ble-l2cap-bumble-") as temp:
        binary = pathlib.Path(temp) / "l2cap-fixture"
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
            request = l2cap.L2CAP_LE_Credit_Based_Connection_Request(
                identifier=7,
                le_psm=0x0027,
                source_cid=0x0041,
                mtu=80,
                mps=40,
                initial_credits=2,
            )
            request_frame = bytes(l2cap.L2CAP_PDU(
                l2cap.L2CAP_LE_SIGNALING_CID, bytes(request)
            ))
            write_record(process.stdin, request_frame)

            response_pdu = l2cap.L2CAP_PDU.from_bytes(read_record(process.stdout))
            assert response_pdu.cid == l2cap.L2CAP_LE_SIGNALING_CID
            response = l2cap.L2CAP_Control_Frame.from_bytes(response_pdu.payload)
            assert isinstance(response, l2cap.L2CAP_LE_Credit_Based_Connection_Response)
            assert response.identifier == request.identifier
            assert response.result == 0
            assert response.destination_cid == 0x0040
            assert response.mtu == 100 and response.mps == 40
            assert response.initial_credits == 2

            # The peer sends one unsegmented SDU K-frame to the C endpoint CID.
            data_frame = bytes(l2cap.L2CAP_PDU(
                response.destination_cid, struct.pack("<H", 5) + b"hello"
            ))
            write_record(process.stdin, data_frame)
            assert read_record(process.stdout) == b"hello"

            credit_pdu = l2cap.L2CAP_PDU.from_bytes(read_record(process.stdout))
            assert credit_pdu.cid == l2cap.L2CAP_LE_SIGNALING_CID
            credit = l2cap.L2CAP_Control_Frame.from_bytes(credit_pdu.payload)
            assert isinstance(credit, l2cap.L2CAP_LE_Flow_Control_Credit)
            assert credit.cid == request.source_cid and credit.credits == 1
            process.stdin.close()
            assert process.wait(timeout=2) == 0
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()
    print("C LE L2CAP CoC ↔ Bumble signaling/data/credit interoperability: PASS")


if __name__ == "__main__":
    main()
