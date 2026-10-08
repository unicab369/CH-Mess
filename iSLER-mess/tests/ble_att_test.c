#include <assert.h>
#include <stdint.h>
#include "../ble_att/ble_att.h"

int main(void) {
    uint8_t encoded[2];
    ble_att_put_u16(encoded, 0x1234);
    assert(encoded[0] == 0x34 && encoded[1] == 0x12);
    assert(ble_att_get_u16(encoded) == 0x1234);

    static const struct {
        uint8_t request;
        uint8_t response;
    } pairs[] = {
        {BLE_ATT_OP_EXCHANGE_MTU_REQ, BLE_ATT_OP_EXCHANGE_MTU_RSP},
        {BLE_ATT_OP_FIND_INFO_REQ, BLE_ATT_OP_FIND_INFO_RSP},
        {BLE_ATT_OP_FIND_BY_TYPE_VALUE_REQ,
         BLE_ATT_OP_FIND_BY_TYPE_VALUE_RSP},
        {BLE_ATT_OP_READ_BY_TYPE_REQ, BLE_ATT_OP_READ_BY_TYPE_RSP},
        {BLE_ATT_OP_READ_REQ, BLE_ATT_OP_READ_RSP},
        {BLE_ATT_OP_READ_BLOB_REQ, BLE_ATT_OP_READ_BLOB_RSP},
        {BLE_ATT_OP_READ_MULTIPLE_REQ, BLE_ATT_OP_READ_MULTIPLE_RSP},
        {BLE_ATT_OP_READ_BY_GROUP_TYPE_REQ,
         BLE_ATT_OP_READ_BY_GROUP_TYPE_RSP},
        {BLE_ATT_OP_WRITE_REQ, BLE_ATT_OP_WRITE_RSP},
        {BLE_ATT_OP_PREPARE_WRITE_REQ, BLE_ATT_OP_PREPARE_WRITE_RSP},
        {BLE_ATT_OP_EXECUTE_WRITE_REQ, BLE_ATT_OP_EXECUTE_WRITE_RSP},
        {BLE_ATT_OP_READ_MULTIPLE_VARIABLE_REQ,
         BLE_ATT_OP_READ_MULTIPLE_VARIABLE_RSP},
    };
    for (unsigned i = 0; i < sizeof(pairs) / sizeof(pairs[0]); i++)
        assert(ble_att_response_opcode(pairs[i].request) == pairs[i].response);

    assert(ble_att_response_opcode(BLE_ATT_OP_WRITE_COMMAND) == 0);
    assert(ble_att_response_opcode(BLE_ATT_OP_SIGNED_WRITE_COMMAND) == 0);
    assert(ble_att_response_opcode(BLE_ATT_OP_HANDLE_VALUE_NOTIFICATION) == 0);
    assert(ble_att_response_opcode(BLE_ATT_OP_HANDLE_VALUE_INDICATION) == 0);
    assert(ble_att_response_opcode(BLE_ATT_OP_MULTIPLE_HANDLE_VALUE_NOTIFICATION)
           == 0);
    assert(ble_att_response_opcode(BLE_ATT_OP_ERROR_RSP) == 0);
    assert(ble_att_response_opcode(BLE_ATT_OP_READ_RSP) == 0);
    return 0;
}
