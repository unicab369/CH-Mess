#include <assert.h>
#include <stdint.h>
#include "../ble_att.h"

int main(void) {
    uint8_t encoded[2];
    ble_att_put_u16(encoded, 0x1234);
    assert(encoded[0] == 0x34 && encoded[1] == 0x12);
    assert(ble_att_get_u16(encoded) == 0x1234);

    static const struct {
        uint8_t request;
        uint8_t response;
    } pairs[] = {
        {ATT_OP_EXCHANGE_MTU_REQ, ATT_OP_EXCHANGE_MTU_RSP},
        {ATT_OP_FIND_INFO_REQ, ATT_OP_FIND_INFO_RSP},
        {ATT_OP_FIND_BY_TYPE_VALUE_REQ,
         ATT_OP_FIND_BY_TYPE_VALUE_RSP},
        {ATT_OP_READ_BY_TYPE_REQ, ATT_OP_READ_BY_TYPE_RSP},
        {ATT_OP_READ_REQ, ATT_OP_READ_RSP},
        {ATT_OP_READ_BLOB_REQ, ATT_OP_READ_BLOB_RSP},
        {ATT_OP_READ_MULTIPLE_REQ, ATT_OP_READ_MULTIPLE_RSP},
        {ATT_OP_READ_BY_GROUP_TYPE_REQ,
         ATT_OP_READ_BY_GROUP_TYPE_RSP},
        {ATT_OP_WRITE_REQ, ATT_OP_WRITE_RSP},
        {ATT_OP_PREPARE_WRITE_REQ, ATT_OP_PREPARE_WRITE_RSP},
        {ATT_OP_EXECUTE_WRITE_REQ, ATT_OP_EXECUTE_WRITE_RSP},
        {ATT_OP_READ_MULTIPLE_VARIABLE_REQ,
         ATT_OP_READ_MULT_VAR_RSP},
    };
    for (unsigned i = 0; i < sizeof(pairs) / sizeof(pairs[0]); i++)
        assert(ble_att_response_opcode(pairs[i].request) == pairs[i].response);

    assert(ble_att_response_opcode(ATT_OP_WRITE_COMMAND) == 0);
    assert(ble_att_response_opcode(ATT_OP_SIGNED_WRITE_COMMAND) == 0);
    assert(ble_att_response_opcode(ATT_OP_HANDLE_VALUE_NOTIFICATION) == 0);
    assert(ble_att_response_opcode(ATT_OP_HANDLE_VALUE_INDICATION) == 0);
    assert(ble_att_response_opcode(ATT_OP_MULT_HANDLE_VALUE_NOTIFICATION)
           == 0);
    assert(ble_att_response_opcode(ATT_OP_ERROR_RSP) == 0);
    assert(ble_att_response_opcode(ATT_OP_READ_RSP) == 0);
    return 0;
}
