#ifndef BLE_GATT_SERVICE_AUDIO_H
#define BLE_GATT_SERVICE_AUDIO_H

// Contents: Microphone/Input Control (MICS/AICS), Volume Control (VCS/VOCS),
// and Media Control (MCS/GMCS).
// BLE Audio is not supported. Omitted LE Audio GATT services:
// ASCS (0x184E): Audio Stream Control; BASS (0x184F): Broadcast Audio Scan;
// PACS (0x1850): Published Audio Capabilities; CAS (0x1853): Common Audio;
// TMAS (0x1855): Telephony and Media Audio; GMAS (0x1858): Gaming Audio.
// LE Audio broadcast service-data UUIDs are also unsupported:
// 0x1851: Basic Audio Announcement; 0x1852: Broadcast Audio Announcement;
// 0x1856: Public Broadcast Announcement. These UUIDs identify advertising
// data, not GATT database services.
// This header contains standalone GATT audio controls and related services.

// Microphone and Input Control: MICS, AICS
// MICS means Microphone Control Service.

typedef struct {
    uint16_t service_handle, mute_handle, mute_cccd_handle;
    uint8_t mute;
    ble_gatt_server *server;
} ble_gatt_mics_service;

enum { GATT_MICS_ATT_ERR_MUTE_DISABLED = 0x80 };

// Internal: validate and apply a MICS Mute write, then notify subscribers.
static uint8_t gatt_mics_mute_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    ble_gatt_mics_service *service = context;

    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;

    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (len != 1)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    if (value[0] > 1)
        return ATT_ERR_VALUE_NOT_ALLOWED;

    ble_gatt_attribute *attribute = gatt_server_find(service->server,
        service->mute_handle);
    uint8_t *stored = attribute ?
        gatt_attribute_value(service->server, attribute) : NULL;

    if (!attribute || !stored || attribute->value_capacity < 1)
        return ATT_ERR_UNLIKELY_ERROR;

    if (stored[0] == 2) return GATT_MICS_ATT_ERR_MUTE_DISABLED;

    uint8_t changed = stored[0] != value[0];
    service->mute = value[0];
    stored[0] = value[0];
    attribute->value_len = 1;

    if (changed)
        (void)gatt_queue_event(service->server, service->mute_handle,
                                      value, 1, 0);
    return 0;
}

// Application-facing API
// Add MICS with its encrypted Mute characteristic. Writes validate and update
// the mute state and notify subscribed clients; local changes use the setter.
static inline int gatt_mics_add_aics(
    ble_gatt_server *server, ble_gatt_mics_service *service,
    uint8_t mute, const uint16_t *aics_service_handles, uint8_t aics_count
) {
    if (!server || !service || mute > 2 ||
        (aics_count && !aics_service_handles))
        return 0;

    for (uint8_t i = 0; i < aics_count; i++) {
        ble_gatt_attribute *included = gatt_server_find(
            server, aics_service_handles[i]);

        if (!included ||
            !(included->flags & BLE_GATT_ATTR_SECONDARY_SERVICE) ||
            gatt_uuid_assigned16(&included->uuid) != 0x1843) return 0;

        for (uint8_t j = 0; j < i; j++)
            if (aics_service_handles[j] == aics_service_handles[i]) return 0;
    }
    memset(service, 0, sizeof(*service));
    service->mute = mute;
    service->server = server;
    ble_gatt_sig_characteristic characteristic = {
        0x2bc3, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE |
            BLE_GATT_PROP_NOTIFY, 1,
        BLE_GATT_PERM_READ_ENC | BLE_GATT_PERM_WRITE_ENC,
        &service->mute, 1
    };
    ble_gatt_sig_handlers handlers[] = {
        {.write = gatt_mics_mute_write, .context = service}
    };
    uint16_t values[1], cccds[1];

    if (!gatt_sig_add_full(server, 0x184d,
            aics_service_handles, aics_count, &characteristic, 1,
            &service->service_handle, values, cccds, handlers)
    ) return 0;

    service->mute_handle = values[0];
    service->mute_cccd_handle = cccds[0];
    return 1;
}

// Add standalone MICS without included AICS instances.
static inline int gatt_mics_add(
    ble_gatt_server *server, ble_gatt_mics_service *service,
    uint8_t mute
) {
    return gatt_mics_add_aics(server, service,
                                                         mute, NULL, 0);
}

// Update MICS Mute after a local microphone-state change and notify clients
// that enabled the characteristic's CCCD. Returns 1 when the value is set;
// notification queueing can fail independently if the queue is full.
static inline int gatt_mics_set_mute(
    ble_gatt_server *server, ble_gatt_mics_service *service,
    uint8_t mute
) {
    if (!server || !service || service->server != server || mute > 2)
        return 0;

    ble_gatt_attribute *attribute = gatt_server_find(server,
        service->mute_handle);
    uint8_t *stored = attribute ? gatt_attribute_value(server, attribute) : NULL;

    if (!attribute || !stored || attribute->value_capacity < 1)
        return 0;

    uint8_t changed = stored[0] != mute;
    service->mute = mute;
    stored[0] = mute;
    attribute->value_len = 1;

    if (changed)
        (void)gatt_queue_event(server, service->mute_handle, &mute, 1, 0);
    return 1;
}

typedef struct {
    uint16_t service_handle;
    uint16_t state_handle, state_cccd_handle;
    uint16_t gain_properties_handle, input_type_handle;
    uint16_t status_handle, status_cccd_handle;
    uint16_t control_point_handle;
    uint16_t description_handle, description_cccd_handle;
    uint8_t state[4], gain_properties[3], status;
    ble_gatt_server *server;
} ble_gatt_aics_service;

enum {
    GATT_AICS_ATT_ERR_INVALID_CHANGE_COUNTER = 0x80,
    GATT_AICS_ATT_ERR_OPCODE_NOT_SUPPORTED = 0x81,
    GATT_AICS_ATT_ERR_MUTE_DISABLED = 0x82,
    GATT_AICS_ATT_ERR_VALUE_OUT_OF_RANGE = 0x83,
    GATT_AICS_ATT_ERR_GAIN_MODE_CHANGE_NOT_ALLOWED = 0x84
};

static int8_t gatt_aics_decode_s8(uint8_t value) {
    return value <= 127 ? (int8_t)value : (int8_t)((int16_t)value - 256);
}

static void gatt_aics_commit_state(
    ble_gatt_aics_service *service,
    const uint8_t state[4]
) {
    if (!service || !service->server || !state ||
        memcmp(service->state, state, 4) == 0) return;

    ble_gatt_attribute *attribute = gatt_server_find(service->server,
        service->state_handle);
    uint8_t *stored = attribute ?
        gatt_attribute_value(service->server, attribute) : NULL;

    if (!attribute || !stored || attribute->value_capacity < 4) return;

    memcpy(service->state, state, 4);
    memcpy(stored, state, 4);
    attribute->value_len = 4;
    (void)gatt_queue_event(service->server, service->state_handle,
                                  state, 4, 0);
}

static uint8_t gatt_aics_cp_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    ble_gatt_aics_service *service = context;

    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;

    if (offset) return ATT_ERR_INVALID_OFFSET;
    if (!len) return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    uint8_t opcode = value[0];

    if (opcode < 1 || opcode > 5)
        return GATT_AICS_ATT_ERR_OPCODE_NOT_SUPPORTED;
    if (len != (opcode == 1 ? 3 : 2))
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    if (value[1] != service->state[3])
        return GATT_AICS_ATT_ERR_INVALID_CHANGE_COUNTER;

    uint8_t next[4];
    memcpy(next, service->state, sizeof(next));
    switch (opcode) {
    case 1: {
        int8_t gain = gatt_aics_decode_s8(value[2]);
        int8_t minimum = gatt_aics_decode_s8(service->gain_properties[1]);
        int8_t maximum = gatt_aics_decode_s8(service->gain_properties[2]);

        if (gain < minimum || gain > maximum)
            return GATT_AICS_ATT_ERR_VALUE_OUT_OF_RANGE;

        if (next[2] == 0 || next[2] == 2) next[0] = value[2];
        break;
    }
    case 2:
    case 3:
        if (next[1] == 2) return GATT_AICS_ATT_ERR_MUTE_DISABLED;

        next[1] = opcode == 2 ? 0 : 1;
        break;
    case 4:
    case 5:
        if (next[2] < 2)
            return GATT_AICS_ATT_ERR_GAIN_MODE_CHANGE_NOT_ALLOWED;

        next[2] = opcode == 4 ? 2 : 3;
        break;
    }

    if (memcmp(next, service->state, sizeof(next)) != 0) {
        next[3]++;
        gatt_aics_commit_state(service, next);
    }

    return 0;
}

static uint8_t gatt_aics_desc_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    ble_gatt_aics_service *service = context;

    if (!service || !service->server || (len && !value))
        return ATT_ERR_UNLIKELY_ERROR;

    if (offset) return ATT_ERR_INVALID_OFFSET;

    if (len > BLE_GATT_ATT_VALUE_MAX)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    ble_gatt_attribute *attribute = gatt_server_find(service->server,
        service->description_handle);
    uint8_t *stored = attribute ?
        gatt_attribute_value(service->server, attribute) : NULL;

    if (!attribute || !stored || len > attribute->value_capacity)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    uint8_t changed = attribute->value_len != len ||
        (len && memcmp(stored, value, len) != 0);

    if (len) memcpy(stored, value, len);
    attribute->value_len = len;

    if (changed)
        (void)gatt_queue_event(service->server,
            service->description_handle, value, len, 0);
    return 0;
}

// Application-facing API
// AICS means Audio Input Control Service.
// Add an AICS secondary service. Add it before its parent service, then add
// its Include declaration to the parent before the parent's characteristics.
// Its five control-point procedures validate Change_Counter, supported modes,
// mute state, and gain bounds, then update and notify Audio Input State.
static inline int gatt_aics_add(
    ble_gatt_server *server, ble_gatt_aics_service *service,
    const uint8_t state[4], const uint8_t gain_properties[3],
    uint8_t input_type, uint8_t status, const uint8_t *description,
    uint16_t description_len, uint16_t description_capacity,
    uint8_t description_writable, uint16_t control_point_capacity
) {
    if (!server || !service || !state || !gain_properties || !description ||
        description_len > description_capacity ||
        description_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        description_writable > 1 || control_point_capacity < 3 ||
        control_point_capacity > BLE_GATT_SERVER_VALUE_MAX
    ) return 0;

    int8_t gain_minimum = gatt_aics_decode_s8(gain_properties[1]);
    int8_t gain_maximum = gatt_aics_decode_s8(gain_properties[2]);
    int8_t initial_gain = gatt_aics_decode_s8(state[0]);

    if (gain_minimum > gain_maximum || initial_gain < gain_minimum ||
        initial_gain > gain_maximum || state[1] > 2 || state[2] > 3 ||
        status > 1) return 0;

    uint16_t old_count = server->count;
    uint16_t old_next_handle = server->next_handle;
    uint16_t old_value_used = server->value_used;
    uint16_t declaration;
    uint8_t control_point_initial = 0;
    ble_gatt_uuid service_uuid = {2, {0x43, 0x18}};
    ble_gatt_uuid state_uuid = {2, {0x77, 0x2b}};
    ble_gatt_uuid gain_uuid = {2, {0x78, 0x2b}};
    ble_gatt_uuid type_uuid = {2, {0x79, 0x2b}};
    ble_gatt_uuid status_uuid = {2, {0x7a, 0x2b}};
    ble_gatt_uuid control_uuid = {2, {0x7b, 0x2b}};
    ble_gatt_uuid description_uuid = {2, {0x7c, 0x2b}};
    ble_gatt_uuid cccd_uuid = {2, {0x02, 0x29}};
    uint16_t service_handle, state_handle, state_cccd;
    uint16_t gain_handle, type_handle, status_handle, status_cccd;
    uint16_t control_handle, description_handle, description_cccd;

    memset(service, 0, sizeof(*service));
    memcpy(service->state, state, 4);
    memcpy(service->gain_properties, gain_properties, 3);
    service->status = status;
    service->server = server;
    int added = gatt_server_add_service(server, &service_uuid, 0,
                                         &service_handle) &&
        gatt_add_characteristic(server, &state_uuid,
            BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY,
            BLE_GATT_PERM_READ_ENC, service->state, 4, 4, NULL, NULL, NULL,
            &declaration, &state_handle) &&
        gatt_add_descriptor(server, &cccd_uuid,
            BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
            NULL, 0, 0, NULL, NULL, NULL, &state_cccd) &&
        gatt_add_characteristic(server, &gain_uuid,
            BLE_GATT_PROP_READ, BLE_GATT_PERM_READ_ENC,
            service->gain_properties, 3, 3, NULL, NULL, NULL, &declaration,
            &gain_handle) &&
        gatt_add_characteristic(server, &type_uuid,
            BLE_GATT_PROP_READ, BLE_GATT_PERM_READ_ENC,
            &input_type, 1, 1, NULL, NULL, NULL, &declaration, &type_handle) &&
        gatt_add_characteristic(server, &status_uuid,
            BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY,
            BLE_GATT_PERM_READ_ENC, &service->status, 1, 1, NULL, NULL, NULL,
            &declaration, &status_handle) &&
        gatt_add_descriptor(server, &cccd_uuid,
            BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
            NULL, 0, 0, NULL, NULL, NULL, &status_cccd) &&
        gatt_add_characteristic(server, &control_uuid,
            BLE_GATT_PROP_WRITE, BLE_GATT_PERM_WRITE_ENC,
            &control_point_initial, 0, control_point_capacity,
            NULL, gatt_aics_cp_write, service,
            &declaration, &control_handle);

    if (added)
        server->attrs[server->count - 1].flags |=
            BLE_GATT_ATTR_VAR_LEN;

    if (added)
        added = gatt_add_characteristic(server, &description_uuid,
            BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY |
                (description_writable ? BLE_GATT_PROP_WRITE_NO_RSP : 0),
            BLE_GATT_PERM_READ_ENC |
                (description_writable ? BLE_GATT_PERM_WRITE_ENC : 0),
            description, description_len, description_capacity,
            NULL, description_writable ? gatt_aics_desc_write : NULL,
            description_writable ? service : NULL, &declaration,
            &description_handle);

    if (added)
        added = gatt_add_descriptor(server, &cccd_uuid,
            BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
            NULL, 0, 0, NULL, NULL, NULL, &description_cccd);

    if (added && description_capacity > description_len)
        server->attrs[server->count - 2].flags |=
            BLE_GATT_ATTR_VAR_LEN;

    if (!added) {
        gatt_service_rollback(server, old_count, old_next_handle,
            old_value_used);
        return 0;
    }

    service->service_handle = service_handle;
    service->state_handle = state_handle;
    service->state_cccd_handle = state_cccd;
    service->gain_properties_handle = gain_handle;
    service->input_type_handle = type_handle;
    service->status_handle = status_handle;
    service->status_cccd_handle = status_cccd;
    service->control_point_handle = control_handle;
    service->description_handle = description_handle;
    service->description_cccd_handle = description_cccd;
    return 1;
}

// Apply an application-owned gain, mute, or mode change and notify clients.
// The Change_Counter advances once when any state field changes.
static inline int gatt_aics_set_state(
    ble_gatt_server *server, ble_gatt_aics_service *service,
    int8_t gain, uint8_t mute, uint8_t gain_mode
) {
    if (!server || !service || service->server != server || mute > 2 ||
        gain_mode > 3 || gain < gatt_aics_decode_s8(service->gain_properties[1]) ||
        gain > gatt_aics_decode_s8(service->gain_properties[2])) return 0;

    uint8_t next[4] = {(uint8_t)gain, mute, gain_mode, service->state[3]};

    if (memcmp(next, service->state, 3) != 0) next[3]++;
    gatt_aics_commit_state(service, next);
    return 1;
}

// Update AICS Audio Input Status and notify clients when it changes.
static inline int gatt_aics_set_status(
    ble_gatt_server *server, ble_gatt_aics_service *service,
    uint8_t status
) {
    if (!server || !service || service->server != server || status > 1)
        return 0;

    ble_gatt_attribute *attribute = gatt_server_find(server, service->status_handle);
    uint8_t *stored = attribute ? gatt_attribute_value(server, attribute) : NULL;

    if (!attribute || !stored || attribute->value_capacity < 1) return 0;

    uint8_t changed = stored[0] != status;
    service->status = stored[0] = status;
    attribute->value_len = 1;

    if (changed) (void)gatt_queue_event(server, service->status_handle,
                                               &status, 1, 0);
    return 1;
}

// Update AICS Audio Input Description after an application-side change.
static inline int gatt_aics_set_desc(
    ble_gatt_server *server, ble_gatt_aics_service *service,
    const uint8_t *description, uint16_t len
) {
    if (!server || !service || service->server != server || (len && !description))
        return 0;

    ble_gatt_attribute *attribute = gatt_server_find(server,
        service->description_handle);
    uint8_t *stored = attribute ? gatt_attribute_value(server, attribute) : NULL;

    if (!attribute || !stored || len > attribute->value_capacity ||
        len > BLE_GATT_ATT_VALUE_MAX) return 0;

    uint8_t changed = attribute->value_len != len ||
        (len && memcmp(stored, description, len) != 0);

    if (len) memcpy(stored, description, len);
    attribute->value_len = len;

    if (changed) (void)gatt_queue_event(server,
        service->description_handle, description, len, 0);
    return 1;
}

// Volume Control: VCS, VOCS
// VCS means Volume Control Service; VOCS means Volume Offset Control Service.
typedef struct {
    uint16_t service_handle, volume_state_handle, volume_state_cccd_handle;
    uint16_t control_point_handle, volume_flags_handle;
    uint16_t volume_flags_cccd_handle;
    uint8_t volume_state[3], volume_flags, step_size;
    uint8_t volume_flags_changeable;
    ble_gatt_server *server;
} ble_gatt_vcs_service;

enum {
    GATT_VCS_ATT_ERR_OPCODE_NOT_SUPPORTED = 0x80,
    GATT_VCS_ATT_ERR_INVALID_CHANGE_COUNTER = 0x81
};

// Internal: commit a changed VCS state and send its required notification.
static void gatt_vcs_apply_state(
    ble_gatt_vcs_service *service, uint8_t volume, uint8_t mute
) {
    if (!service || !service->server || mute > 1 ||
        (volume == service->volume_state[0] &&
         mute == service->volume_state[1])
    ) return;

    ble_gatt_attribute *attribute = gatt_server_find(service->server,
        service->volume_state_handle);
    uint8_t *stored = attribute ?
        gatt_attribute_value(service->server, attribute) : NULL;

    if (!attribute || !stored || attribute->value_capacity < 3)
        return;

    service->volume_state[0] = volume;
    service->volume_state[1] = mute;
    service->volume_state[2]++;
    memcpy(stored, service->volume_state, 3);
    attribute->value_len = 3;
    (void)gatt_queue_event(service->server,
        service->volume_state_handle, service->volume_state, 3, 0);
}

// Internal: process the VCS Volume Control Point request and Change_Counter.
static uint8_t gatt_vcs_cp_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    ble_gatt_vcs_service *service = context;

    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;

    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (!len)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    uint8_t opcode = value[0];

    if (opcode > 6)
        return GATT_VCS_ATT_ERR_OPCODE_NOT_SUPPORTED;

    uint16_t expected_len = opcode == 4 ? 3 : 2;

    if (len != expected_len)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    if (value[1] != service->volume_state[2])
        return GATT_VCS_ATT_ERR_INVALID_CHANGE_COUNTER;

    uint8_t volume = service->volume_state[0];
    uint8_t mute = service->volume_state[1];
    uint8_t step = service->step_size ? service->step_size : 1;
    switch (opcode) {
    case 0: // Relative Volume Down
        volume = volume > step ? (uint8_t)(volume - step) : 0;
        break;
    case 1: // Relative Volume Up
        volume = volume > (uint8_t)(255 - step) ? 255 :
                 (uint8_t)(volume + step);
        break;
    case 2: // Unmute/Relative Volume Down
        volume = volume > step ? (uint8_t)(volume - step) : 0;
        mute = 0;
        break;
    case 3: // Unmute/Relative Volume Up
        volume = volume > (uint8_t)(255 - step) ? 255 :
                 (uint8_t)(volume + step);
        mute = 0;
        break;
    case 4: // Set Absolute Volume
        volume = value[2];
        break;
    case 5: // Unmute
        mute = 0;
        break;
    case 6: // Mute
        mute = 1;
        break;
    }
    gatt_vcs_apply_state(service, volume, mute);
    return 0;
}

// Application-facing API
// Add VCS state, control point, and flags. The control point validates all
// seven opcodes and Change_Counter operands and applies a step size of one.
static inline int gatt_vcs_add(
    ble_gatt_server *server, ble_gatt_vcs_service *service,
    const uint8_t volume_state[3], uint8_t volume_flags,
    uint8_t volume_flags_changeable
) {
    if (!server || !service || !volume_state || volume_state[1] > 1)
        return 0;

    memset(service, 0, sizeof(*service));
    memcpy(service->volume_state, volume_state, 3);
    service->volume_flags = volume_flags;
    service->step_size = 1;
    service->volume_flags_changeable = volume_flags_changeable != 0;
    service->server = server;
    ble_gatt_sig_characteristic characteristics[3] = {
        {0x2b7d, BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY, 1,
            BLE_GATT_PERM_READ_ENC, service->volume_state, 3},
        {0x2b7e, BLE_GATT_PROP_WRITE, 0,
            BLE_GATT_PERM_WRITE_ENC, NULL, 0, 3, 1},
        {0x2b7f, BLE_GATT_PROP_READ |
            (volume_flags_changeable ? BLE_GATT_PROP_NOTIFY : 0),
            volume_flags_changeable, BLE_GATT_PERM_READ_ENC,
            &service->volume_flags, 1}
    };
    ble_gatt_sig_handlers handlers[3] = {0};
    handlers[1].write = gatt_vcs_cp_write;
    handlers[1].context = service;
    uint16_t values[3], cccds[3];

    if (!gatt_sig_add_handlers(server, 0x1844, characteristics,
            3, &service->service_handle, values, cccds, handlers)
    ) return 0;

    service->volume_state_handle = values[0];
    service->volume_state_cccd_handle = cccds[0];
    service->control_point_handle = values[1];
    service->volume_flags_handle = values[2];
    service->volume_flags_cccd_handle = cccds[2];
    return 1;
}

// Update VCS state after a local audio change and notify subscribed clients.
// The Change_Counter advances once only when Volume_Setting or Mute changes.
static inline int gatt_vcs_set_state(
    ble_gatt_server *server, ble_gatt_vcs_service *service,
    uint8_t volume, uint8_t mute
) {
    if (!server || !service || service->server != server || mute > 1)
        return 0;

    gatt_vcs_apply_state(service, volume, mute);
    return 1;
}

typedef struct {
    uint16_t service_handle, state_handle, state_cccd_handle;
    uint16_t location_handle, location_cccd_handle;
    uint16_t control_point_handle, description_handle, description_cccd_handle;
    uint8_t state[3];
    ble_gatt_server *server;
} ble_gatt_vocs_service;

enum {
    GATT_VOCS_ATT_ERR_OPCODE_NOT_SUPPORTED = 0x80,
    GATT_VOCS_ATT_ERR_INVALID_CHANGE_COUNTER = 0x81,
    GATT_VOCS_ATT_ERR_VALUE_OUT_OF_RANGE = 0x82
};

static int16_t gatt_vocs_decode_offset(const uint8_t value[2]) {
    uint16_t raw = ble_get_le16(value);
    int32_t signed_value = (raw & 0x8000u) ? (int32_t)raw - 65536 : raw;
    return (int16_t)signed_value;
}

// Internal: process the VOCS Set Volume Offset request.
static uint8_t gatt_vocs_cp_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    ble_gatt_vocs_service *service = context;

    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;

    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (!len)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    if (value[0] != 1)
        return GATT_VOCS_ATT_ERR_OPCODE_NOT_SUPPORTED;
    if (len != 4)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    if (value[1] != service->state[2])
        return GATT_VOCS_ATT_ERR_INVALID_CHANGE_COUNTER;

    int16_t requested = gatt_vocs_decode_offset(value + 2);

    if (requested < -255 || requested > 255)
        return GATT_VOCS_ATT_ERR_VALUE_OUT_OF_RANGE;

    int16_t current = gatt_vocs_decode_offset(service->state);

    if (requested == current) return 0;

    ble_gatt_attribute *attribute = gatt_server_find(service->server,
        service->state_handle);
    uint8_t *stored = attribute ?
        gatt_attribute_value(service->server, attribute) : NULL;

    if (!attribute || !stored || attribute->value_capacity < 3)
        return ATT_ERR_UNLIKELY_ERROR;

    ble_put_le16(service->state, (uint16_t)requested);
    service->state[2]++;
    memcpy(stored, service->state, 3);
    attribute->value_len = 3;
    (void)gatt_queue_event(service->server, service->state_handle,
                                  service->state, 3, 0);
    return 0;
}

// Application-facing API
// Add one VOCS secondary service. Register it before the parent service and
// include it there. The Set Volume Offset procedure validates its change
// counter and range, updates the state, and notifies subscribed clients.
static inline int gatt_vocs_add(
    ble_gatt_server *server,
    ble_gatt_vocs_service *service,
    const uint8_t state[3], const uint8_t audio_location[4],
    uint8_t control_point_capacity, const uint8_t *description,
    uint16_t description_len, uint16_t description_capacity
) {
    if (!server || !service || !state || !audio_location || !description ||
        control_point_capacity < 4 ||
        gatt_vocs_decode_offset(state) < -255 ||
        gatt_vocs_decode_offset(state) > 255 ||
        description_len > description_capacity ||
        description_capacity > BLE_GATT_SERVER_VALUE_MAX
    ) return 0;

    uint16_t old_count = server->count;
    uint16_t old_next_handle = server->next_handle;
    uint16_t old_value_used = server->value_used;
    uint16_t declaration;
    uint8_t control_point_initial = 0;
    uint16_t service_handle, state_handle, state_cccd;
    uint16_t location_handle, location_cccd;
    uint16_t control_point_handle, description_handle, description_cccd;
    ble_gatt_uuid service_uuid = {2, {0x45, 0x18}};
    ble_gatt_uuid state_uuid = {2, {0x80, 0x2b}};
    ble_gatt_uuid location_uuid = {2, {0x81, 0x2b}};
    ble_gatt_uuid control_point_uuid = {2, {0x82, 0x2b}};
    ble_gatt_uuid description_uuid = {2, {0x83, 0x2b}};
    ble_gatt_uuid cccd_uuid = {2, {0x02, 0x29}};
    memset(service, 0, sizeof(*service));
    memcpy(service->state, state, 3);
    service->server = server;

    int added = gatt_server_add_service(server, &service_uuid, 0,
                                         &service_handle) &&
        gatt_add_characteristic(server, &state_uuid,
            BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY,
            BLE_GATT_PERM_READ_ENC, state, 3, 3, NULL, NULL, NULL,
            &declaration, &state_handle) &&
        gatt_add_descriptor(server, &cccd_uuid,
            BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
            NULL, 0, 0, NULL, NULL, NULL, &state_cccd) &&
        gatt_add_characteristic(server, &location_uuid,
            BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY,
            BLE_GATT_PERM_READ_ENC, audio_location, 4, 4,
            NULL, NULL, NULL, &declaration, &location_handle) &&
        gatt_add_descriptor(server, &cccd_uuid,
            BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
            NULL, 0, 0, NULL, NULL, NULL, &location_cccd) &&
        gatt_add_characteristic(server, &control_point_uuid,
            BLE_GATT_PROP_WRITE, BLE_GATT_PERM_WRITE_ENC,
            &control_point_initial, 0, control_point_capacity,
            NULL, gatt_vocs_cp_write, service,
            &declaration, &control_point_handle);

    if (added)
        server->attrs[server->count - 1].flags |=
            BLE_GATT_ATTR_VAR_LEN;

    if (added)
        added = gatt_add_characteristic(server, &description_uuid,
            BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY,
            BLE_GATT_PERM_READ_ENC, description, description_len,
            description_capacity, NULL, NULL, NULL, &declaration,
            &description_handle);

    if (added)
        added = gatt_add_descriptor(server, &cccd_uuid,
            BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
            NULL, 0, 0, NULL, NULL, NULL, &description_cccd);

    if (added && description_capacity > description_len)
        server->attrs[server->count - 2].flags |=
            BLE_GATT_ATTR_VAR_LEN;

    if (!added) {
        gatt_service_rollback(server, old_count, old_next_handle,
            old_value_used);
        return 0;
    }

    service->service_handle = service_handle;
    service->state_handle = state_handle;
    service->state_cccd_handle = state_cccd;
    service->location_handle = location_handle;
    service->location_cccd_handle = location_cccd;
    service->control_point_handle = control_point_handle;
    service->description_handle = description_handle;
    service->description_cccd_handle = description_cccd;
    return 1;
}

// Update VOCS state after a local audio-output offset change and notify clients.
static inline int gatt_vocs_set_offset(
    ble_gatt_server *server,
    ble_gatt_vocs_service *service,
    int16_t offset
) {
    if (!server || !service || service->server != server ||
        offset < -255 || offset > 255
    ) return 0;

    int16_t current = gatt_vocs_decode_offset(service->state);

    if (offset == current) return 1;

    ble_gatt_attribute *attribute = gatt_server_find(server,
        service->state_handle);
    uint8_t *stored = attribute ? gatt_attribute_value(server, attribute) : NULL;

    if (!attribute || !stored || attribute->value_capacity < 3)
        return 0;

    ble_put_le16(service->state, (uint16_t)offset);
    service->state[2]++;
    memcpy(stored, service->state, 3);
    attribute->value_len = 3;
    (void)gatt_queue_event(server, service->state_handle,
                                  service->state, 3, 0);
    return 1;
}

// Media Control: MCS, GMCS
// MCS means Media Control Service; GMCS means Generic Media Control Service.
typedef struct {
    void *service;
    uint8_t index;
} ble_gatt_mcs_object_ctx;

typedef struct {
    uint16_t service_handle;
    uint16_t player_name_handle, player_name_cccd_handle;
    uint16_t track_changed_handle, changed_cccd_handle;
    uint16_t track_title_handle, title_cccd_handle;
    uint16_t track_duration_handle, duration_cccd_handle;
    uint16_t track_position_handle, position_cccd_handle;
    uint16_t media_state_handle, media_cccd_handle;
    uint16_t ccid_handle;
    uint16_t playback_handle, playback_cccd_handle;
    uint16_t seeking_handle, seeking_cccd_handle;
    uint16_t order_handle, order_cccd_handle;
    uint16_t orders_handle;
    uint16_t control_point_handle, cp_cccd_handle;
    uint16_t cp_opcodes_handle;
    uint16_t object_handle[7], object_cccd_handle[7];
    uint16_t icon_url_handle;
    uint16_t search_cp_handle, search_cp_cccd_handle;
    uint8_t search_pending, search_request_len;
    uint8_t search_request[64];
    ble_gatt_mcs_object_ctx object_ctx[7];
    uint16_t orders_supported;
    uint32_t cp_opcode_mask;
    uint8_t cp_pending, cp_opcode;
    uint8_t cp_param_len, cp_param[4];
    uint8_t object_write_pending, object_write_index;
    uint8_t object_write_len, object_write_value[6];
    uint8_t object_prev_len, object_prev[6];
    ble_gatt_server *server;
} ble_gatt_mcs_service;

// Internal: ignore unsupported playing-order values as required by MCS.
static uint8_t gatt_mcs_order_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    ble_gatt_mcs_service *service = context;

    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;

    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (len != 1)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    uint8_t order = value[0];

    if (order < 1 || order > 10 || (service->orders_supported &&
        !(service->orders_supported & (1u << (order - 1))))
    ) return 0;

    ble_gatt_attribute *attribute = gatt_server_find(
        service->server, service->order_handle);
    uint8_t *stored = attribute ?
        gatt_attribute_value(service->server, attribute) : NULL;

    if (!stored || attribute->value_capacity < 1)
        return ATT_ERR_UNLIKELY_ERROR;

    stored[0] = order;
    return 0;
}

typedef struct {
    const uint8_t *value;
    uint16_t value_len, value_capacity;
} ble_gatt_mcs_string;

typedef struct {
    ble_gatt_mcs_string player_name;
    ble_gatt_mcs_string track_title;
    const uint8_t *track_duration;
    const uint8_t *track_position;
    uint8_t media_state;
    uint8_t ccid;
    uint8_t generic_service;
    uint8_t player_name_notify;
} ble_gatt_mcs_config;

typedef struct {
    uint8_t characteristic_mask;
    uint16_t notify_mask;
    const uint8_t *playback_speed;
    const uint8_t *seeking_speed;
    const uint8_t *playing_order;
    const uint8_t *playing_orders; // little-endian uint16 bitfield
    uint16_t cp_capacity;
    const uint8_t *cp_opcodes; // little-endian uint32 bitfield
    uint16_t ots_service_handle;
    uint8_t object_mask, object_notify_mask;
    ble_gatt_mcs_string object_ids[7];
    ble_gatt_mcs_string icon_url;
    uint16_t search_cp_capacity;
} ble_gatt_mcs_optional_config;

enum {
    GATT_MCS_OPTION_PLAYBACK_SPEED = 0x01,
    GATT_MCS_OPTION_SEEKING_SPEED = 0x02,
    GATT_MCS_OPTION_PLAYING_ORDER = 0x04,
    GATT_MCS_OPTION_PLAYING_ORDERS_SUPPORTED = 0x08,
    GATT_MCS_OPTION_CONTROL_POINT = 0x10,
    GATT_MCS_OPTION_CONTROL_POINT_OPCODES = 0x20,
    GATT_MCS_OPTION_MEDIA_PLAYER_ICON_URL = 0x40,
    GATT_MCS_OPTION_SEARCH_CONTROL_POINT = 0x80
};

enum {
    GATT_MCS_NOTIFY_TRACK_TITLE = 0x0100,
    GATT_MCS_NOTIFY_TRACK_DURATION = 0x0200,
    GATT_MCS_NOTIFY_TRACK_POSITION = 0x0400
};

enum {
    GATT_MCS_OBJECT_PLAYER_ICON = 0,
    GATT_MCS_OBJECT_TRACK_SEGMENTS = 1,
    GATT_MCS_OBJECT_CURRENT_TRACK = 2,
    GATT_MCS_OBJECT_NEXT_TRACK = 3,
    GATT_MCS_OBJECT_PARENT_GROUP = 4,
    GATT_MCS_OBJECT_CURRENT_GROUP = 5,
    GATT_MCS_OBJECT_SEARCH_RESULTS = 6,
    GATT_MCS_SEARCH_SUCCESS = 0x01,
    GATT_MCS_SEARCH_FAILURE = 0x02
};

enum {
    GATT_MCS_RESULT_SUCCESS = 0x01,
    GATT_MCS_RESULT_OPCODE_NOT_SUPPORTED = 0x02,
    GATT_MCS_RESULT_MEDIA_PLAYER_INACTIVE = 0x03,
    GATT_MCS_RESULT_COMMAND_CANNOT_BE_COMPLETED = 0x04
};

static inline int gatt_mcs_opcode_bit(uint8_t opcode) {
    switch (opcode) {
    case 0x01: return 0; case 0x02: return 1; case 0x03: return 2;
    case 0x04: return 3; case 0x05: return 4; case 0x10: return 5;
    case 0x20: return 6; case 0x21: return 7; case 0x22: return 8;
    case 0x23: return 9; case 0x24: return 10; case 0x30: return 11;
    case 0x31: return 12; case 0x32: return 13; case 0x33: return 14;
    case 0x34: return 15; case 0x40: return 16; case 0x41: return 17;
    case 0x42: return 18; case 0x43: return 19; case 0x44: return 20;
    default: return -1;
    }
}

// Internal: capture a validated MCS opcode request for the application.
static uint8_t gatt_mcs_cp_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    ble_gatt_mcs_service *service = context;

    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;

    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (!len || len > 5)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    ble_gatt_attribute *attribute = gatt_server_find(
        service->server, service->control_point_handle);

    if (!attribute || len > attribute->value_capacity)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    ble_gatt_attribute *cccd = gatt_server_cccd_for(
        service->server, service->control_point_handle);

    if (!cccd || !(cccd->cccd & 1))
        return 0xfd;
    if (service->cp_pending)
        return 0xfe;

    uint8_t opcode = value[0];
    int bit = gatt_mcs_opcode_bit(opcode);
    uint8_t supported = bit >= 0 &&
        (service->cp_opcode_mask & (1u << bit));
    uint8_t has_parameter = opcode == 0x10 || opcode == 0x24 ||
                            opcode == 0x34 || opcode == 0x44;

    if (supported && len != (has_parameter ? 5 : 1))
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    service->cp_opcode = opcode;
    service->cp_param_len = (uint8_t)(len - 1);

    if (len > 1)
        memcpy(service->cp_param, value + 1, len - 1);
    service->cp_pending = 1;
    return 0;
}

static uint8_t gatt_mcs_object_id_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    ble_gatt_mcs_object_ctx *write_context = context;
    ble_gatt_mcs_service *service = write_context ?
        write_context->service : NULL;

    if (!service || !service->server || !value || write_context->index >= 7)
        return ATT_ERR_UNLIKELY_ERROR;

    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (len != 0 && len != 6)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    ble_gatt_attribute *attribute = gatt_server_find(service->server,
        service->object_handle[write_context->index]);
    uint8_t *stored = attribute ?
        gatt_attribute_value(service->server, attribute) : NULL;

    if (!attribute || !stored || len > attribute->value_capacity)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    if (service->object_write_pending)
        return 0xfe;

    service->object_prev_len = (uint8_t)attribute->value_len;

    if (attribute->value_len == 6)
        memcpy(service->object_prev, stored, 6);

    if (len) memcpy(stored, value, len);
    attribute->value_len = len;
    service->object_write_pending = 1;
    service->object_write_index = write_context->index;
    service->object_write_len = (uint8_t)len;

    if (len)
        memcpy(service->object_write_value, value, len);
    return 0;
}

static uint8_t gatt_mcs_search_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    ble_gatt_mcs_service *service = context;

    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;

    if (offset)
        return ATT_ERR_INVALID_OFFSET;

    if (len > sizeof(service->search_request))
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    ble_gatt_attribute *attribute = gatt_server_find(service->server,
        service->search_cp_handle);

    if (!attribute || len > attribute->value_capacity)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    ble_gatt_attribute *cccd = gatt_server_cccd_for(service->server,
        service->search_cp_handle);

    if (!cccd || !(cccd->cccd & 1))
        return 0xfd;

    if (service->search_pending)
        return 0xfe;

    uint16_t offset_in_value = 0;
    while (offset_in_value < len) {
        uint8_t item_len = value[offset_in_value];

        if (!item_len || item_len > len - offset_in_value - 1)
            return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

        uint8_t type = value[offset_in_value + 1];

        if ((type == 0x08 || type == 0x09) && item_len != 1)
            return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

        offset_in_value = (uint16_t)(offset_in_value + 1 + item_len);
    }

    if (len)
        memcpy(service->search_request, value, len);
    service->search_request_len = (uint8_t)len;
    service->search_pending = 1;
    return 0;
}

// Application-facing API
// Add the mandatory MCS/GMCS media metadata layout. Set generic_service for
// GMCS; its Media Player Name notification is mandatory. The extended variant
// can add playback, order, and Media Control Point characteristics.
static inline int gatt_mcs_add_full(
    ble_gatt_server *server, ble_gatt_mcs_service *service,
    const ble_gatt_mcs_config *config,
    const ble_gatt_mcs_optional_config *optional
) {
    if (!server || !service || !config ||
        (config->player_name.value_len && !config->player_name.value) ||
        config->player_name.value_len > config->player_name.value_capacity ||
        !config->player_name.value_capacity ||
        config->player_name.value_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        config->player_name_notify > 1 || config->generic_service > 1 ||
        (config->generic_service && !config->player_name_notify) ||
        (config->track_title.value_len && !config->track_title.value) ||
        config->track_title.value_len > config->track_title.value_capacity ||
        !config->track_title.value_capacity ||
        config->track_title.value_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        !config->track_duration || !config->track_position ||
        config->media_state > 3
    ) return 0;

    uint8_t option_mask = optional ? optional->characteristic_mask : 0;
    uint16_t notify_mask = optional ? optional->notify_mask : 0;
    const uint8_t *mcp_opcodes = optional ? optional->cp_opcodes : NULL;
    uint32_t supported_mcp_opcodes = mcp_opcodes ?
        (uint32_t)mcp_opcodes[0] | ((uint32_t)mcp_opcodes[1] << 8) |
        ((uint32_t)mcp_opcodes[2] << 16) |
        ((uint32_t)mcp_opcodes[3] << 24) : 0;
    uint16_t optional_notify_mask = (uint16_t)(notify_mask & 0x002f);

    if ((notify_mask & (uint16_t)~0x072f) ||
        (optional_notify_mask & (uint16_t)~option_mask) ||
        ((option_mask & GATT_MCS_OPTION_PLAYBACK_SPEED) &&
         !optional->playback_speed) ||
        ((option_mask & GATT_MCS_OPTION_SEEKING_SPEED) &&
         !optional->seeking_speed) ||
        ((option_mask & GATT_MCS_OPTION_PLAYING_ORDER) &&
         (!optional->playing_order || *optional->playing_order < 1 ||
          *optional->playing_order > 10)) ||
        ((option_mask & GATT_MCS_OPTION_PLAYING_ORDERS_SUPPORTED) &&
         (!optional->playing_orders ||
          !ble_get_le16(optional->playing_orders) ||
          (ble_get_le16(optional->playing_orders) & 0xfc00))) ||
        ((option_mask & (GATT_MCS_OPTION_PLAYING_ORDER |
                         GATT_MCS_OPTION_PLAYING_ORDERS_SUPPORTED)) ==
         (GATT_MCS_OPTION_PLAYING_ORDER |
          GATT_MCS_OPTION_PLAYING_ORDERS_SUPPORTED) &&
         !(ble_get_le16(optional->playing_orders) &
           (1u << (*optional->playing_order - 1))))
    ) return 0;

    uint8_t mcp_options = (uint8_t)(option_mask &
        (GATT_MCS_OPTION_CONTROL_POINT |
         GATT_MCS_OPTION_CONTROL_POINT_OPCODES));

    if (mcp_options && mcp_options !=
        (GATT_MCS_OPTION_CONTROL_POINT |
         GATT_MCS_OPTION_CONTROL_POINT_OPCODES)
    ) return 0;

    if (mcp_options && (!mcp_opcodes || optional->cp_capacity < 5 ||
        optional->cp_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        !supported_mcp_opcodes || (supported_mcp_opcodes & 0xffe00000))
    ) return 0;

    uint8_t object_mask = optional ? optional->object_mask : 0;
    uint8_t object_notify_mask = optional ?
        optional->object_notify_mask : 0;
    uint8_t search_enabled =
        (option_mask & GATT_MCS_OPTION_SEARCH_CONTROL_POINT) != 0;
    uint8_t ots_needed = object_mask || search_enabled;
    const uint8_t track_object_dependencies =
        (uint8_t)((1u << GATT_MCS_OBJECT_TRACK_SEGMENTS) |
                  (1u << GATT_MCS_OBJECT_NEXT_TRACK) |
                  (1u << GATT_MCS_OBJECT_PARENT_GROUP) |
                  (1u << GATT_MCS_OBJECT_CURRENT_GROUP));

    if ((object_mask & 0x80) || (object_notify_mask & 0x83) ||
        (object_notify_mask & ~object_mask) ||
        ((object_mask & (1u << GATT_MCS_OBJECT_SEARCH_RESULTS)) != 0) !=
            (search_enabled != 0) ||
        ((object_mask & (1u << GATT_MCS_OBJECT_CURRENT_TRACK)) &&
         (object_mask & track_object_dependencies) !=
             track_object_dependencies) ||
        (!(object_mask & (1u << GATT_MCS_OBJECT_CURRENT_TRACK)) &&
         (object_mask & track_object_dependencies)) ||
        ((ots_needed != 0) &&
         (!optional || !optional->ots_service_handle ||
          !gatt_server_find(server, optional->ots_service_handle) ||
          !(gatt_server_find(server, optional->ots_service_handle)->flags &
            BLE_GATT_ATTR_SECONDARY_SERVICE) ||
          gatt_uuid_assigned16(&gatt_server_find(server,
              optional->ots_service_handle)->uuid) != 0x1825)) ||
        ((option_mask & GATT_MCS_OPTION_MEDIA_PLAYER_ICON_URL) &&
         ((optional->icon_url.value_len &&
           !optional->icon_url.value) ||
          !optional->icon_url.value_capacity ||
          optional->icon_url.value_len >
              optional->icon_url.value_capacity ||
          optional->icon_url.value_capacity >
              BLE_GATT_SERVER_VALUE_MAX)) ||
        (search_enabled && (!optional->search_cp_capacity ||
          optional->search_cp_capacity > 64))
    ) return 0;

    for (uint8_t i = 0; i < 7; i++) {
        if (!(object_mask & (1u << i)))
            continue;

        const ble_gatt_mcs_string *object =
            &optional->object_ids[i];

        if ((object->value_len && !object->value) ||
            (object->value_len != 0 && object->value_len != 6)
        ) return 0;
    }

    static const uint8_t empty_value = 0;
    uint8_t name_notify = config->generic_service || config->player_name_notify;
    ble_gatt_sig_characteristic characteristics[22] = {
        {0x2b93, BLE_GATT_PROP_READ |
                (name_notify ? BLE_GATT_PROP_NOTIFY : 0), name_notify,
            BLE_GATT_PERM_READ_ENC, config->player_name.value,
            config->player_name.value_len, config->player_name.value_capacity,
            1},
        {0x2b96, BLE_GATT_PROP_NOTIFY, 1, 0, &empty_value, 0},
        {0x2b97, BLE_GATT_PROP_READ |
                ((notify_mask & GATT_MCS_NOTIFY_TRACK_TITLE) ?
                    BLE_GATT_PROP_NOTIFY : 0),
            (notify_mask & GATT_MCS_NOTIFY_TRACK_TITLE) != 0,
            BLE_GATT_PERM_READ_ENC,
            config->track_title.value, config->track_title.value_len,
            config->track_title.value_capacity, 1},
        {0x2b98, BLE_GATT_PROP_READ |
                ((notify_mask & GATT_MCS_NOTIFY_TRACK_DURATION) ?
                    BLE_GATT_PROP_NOTIFY : 0),
            (notify_mask & GATT_MCS_NOTIFY_TRACK_DURATION) != 0,
            BLE_GATT_PERM_READ_ENC,
            config->track_duration, 4},
        {0x2b99, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE |
                BLE_GATT_PROP_WRITE_NO_RSP |
                ((notify_mask & GATT_MCS_NOTIFY_TRACK_POSITION) ?
                    BLE_GATT_PROP_NOTIFY : 0),
            (notify_mask & GATT_MCS_NOTIFY_TRACK_POSITION) != 0,
            BLE_GATT_PERM_READ_ENC | BLE_GATT_PERM_WRITE_ENC,
            config->track_position, 4},
        {0x2ba3, BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY, 1,
            BLE_GATT_PERM_READ_ENC, &config->media_state, 1},
        {0x2ba8, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ_ENC,
            &config->ccid, 1}
    };
    static const uint16_t optional_uuid[6] = {0x2b9a, 0x2b9b, 0x2ba1,
        0x2ba2, 0x2ba4, 0x2ba5};
    const uint8_t *optional_value[6] = {
        optional ? optional->playback_speed : NULL,
        optional ? optional->seeking_speed : NULL,
        optional ? optional->playing_order : NULL,
        optional ? optional->playing_orders : NULL,
        &empty_value,
        optional ? optional->cp_opcodes : NULL
    };
    uint8_t extra_count = 0;
    for (uint8_t i = 0; i < 6; i++) {
        uint8_t bit = (uint8_t)(1u << i);

        if (!(option_mask & bit))
            continue;

        uint8_t notify = (notify_mask & bit) != 0;
        uint8_t writable = i == 0 || i == 2;
        uint8_t control_point = i == 4;
        uint8_t properties = control_point ?
            BLE_GATT_PROP_WRITE | BLE_GATT_PROP_WRITE_NO_RSP |
                BLE_GATT_PROP_NOTIFY :
            BLE_GATT_PROP_READ |
                (writable ? BLE_GATT_PROP_WRITE |
                           BLE_GATT_PROP_WRITE_NO_RSP : 0) |
                (notify ? BLE_GATT_PROP_NOTIFY : 0);
        uint16_t value_len = i == 3 ? 2 : (i == 5 ? 4 : 1);
        uint16_t value_capacity = control_point ?
            optional->cp_capacity : value_len;
        uint8_t permissions = (control_point ? 0 :
            BLE_GATT_PERM_READ_ENC) |
            (writable || control_point ? BLE_GATT_PERM_WRITE_ENC : 0);
        characteristics[7 + extra_count++] = (ble_gatt_sig_characteristic){
            optional_uuid[i], properties, control_point || notify,
            permissions, optional_value[i], control_point ? 0 : value_len,
            value_capacity, control_point
        };
    }
    static const uint16_t object_uuid[7] = {0x2b94, 0x2b9c, 0x2b9d,
        0x2b9e, 0x2b9f, 0x2ba0, 0x2ba6};
    uint8_t object_indexes[7] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    for (uint8_t i = 0; i < 7; i++) {
        uint8_t object_bit = (uint8_t)(1u << i);

        if (!(object_mask & object_bit))
            continue;

        object_indexes[i] = (uint8_t)(7 + extra_count);
        uint8_t writable = i == GATT_MCS_OBJECT_CURRENT_TRACK ||
            i == GATT_MCS_OBJECT_NEXT_TRACK ||
            i == GATT_MCS_OBJECT_CURRENT_GROUP;
        uint8_t notify = (object_notify_mask & object_bit) != 0 ||
            i == GATT_MCS_OBJECT_SEARCH_RESULTS;
        uint8_t properties = BLE_GATT_PROP_READ |
            (writable ? BLE_GATT_PROP_WRITE | BLE_GATT_PROP_WRITE_NO_RSP : 0) |
            (notify ? BLE_GATT_PROP_NOTIFY : 0);
        const ble_gatt_mcs_string *object =
            &optional->object_ids[i];
        characteristics[7 + extra_count++] = (ble_gatt_sig_characteristic){
            object_uuid[i], properties, notify,
            BLE_GATT_PERM_READ_ENC |
                (writable ? BLE_GATT_PERM_WRITE_ENC : 0),
            object->value_len ? object->value : &empty_value,
            object->value_len, 6, 1
        };
    }
    uint8_t icon_url_index = 0xff;

    if (option_mask & GATT_MCS_OPTION_MEDIA_PLAYER_ICON_URL) {
        icon_url_index = (uint8_t)(7 + extra_count);
        const ble_gatt_mcs_string *url =
            &optional->icon_url;
        characteristics[7 + extra_count++] = (ble_gatt_sig_characteristic){
            0x2b95, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ_ENC,
            url->value_len ? url->value : &empty_value, url->value_len,
            url->value_capacity, 1
        };
    }

    uint8_t search_index = 0xff;

    if (search_enabled) {
        search_index = (uint8_t)(7 + extra_count);
        characteristics[7 + extra_count++] = (ble_gatt_sig_characteristic){
            0x2ba7, BLE_GATT_PROP_WRITE | BLE_GATT_PROP_WRITE_NO_RSP |
                    BLE_GATT_PROP_NOTIFY,
            1, BLE_GATT_PERM_WRITE_ENC, &empty_value, 0,
            optional->search_cp_capacity, 1
        };
    }

    uint8_t characteristic_count = (uint8_t)(7 + extra_count);
    uint16_t value_handles[22], cccd_handles[22];
    ble_gatt_sig_handlers handlers[22] = {0};
    memset(service, 0, sizeof(*service));
    service->server = server;

    if (option_mask & GATT_MCS_OPTION_PLAYING_ORDERS_SUPPORTED)
        service->orders_supported =
            ble_get_le16(optional->playing_orders);
    uint8_t extra_index = 0;
    for (uint8_t i = 0; i < 6; i++) {
        uint8_t bit = (uint8_t)(1u << i);

        if (!(option_mask & bit))
            continue;

        if (i == 2) {
            handlers[7 + extra_index].write =
                gatt_mcs_order_write;
            handlers[7 + extra_index].context = service;
        }
        else if (i == 4) {
            handlers[7 + extra_index].write =
                gatt_mcs_cp_write;
            handlers[7 + extra_index].context = service;
        }

        extra_index++;
    }

    for (uint8_t i = 0; i < 7; i++) {
        if (object_indexes[i] == 0xff ||
            (i != GATT_MCS_OBJECT_CURRENT_TRACK &&
             i != GATT_MCS_OBJECT_NEXT_TRACK &&
             i != GATT_MCS_OBJECT_CURRENT_GROUP)
        ) continue;

        service->object_ctx[i].service = service;
        service->object_ctx[i].index = i;
        handlers[object_indexes[i]].write =
            gatt_mcs_object_id_write;
        handlers[object_indexes[i]].context =
            &service->object_ctx[i];
    }

    if (search_enabled) {
        handlers[search_index].write = gatt_mcs_search_write;
        handlers[search_index].context = service;
    }

    uint16_t include_handles[1];
    uint8_t include_count = ots_needed ? 1 : 0;

    if (include_count)
        include_handles[0] = optional->ots_service_handle;

    if (!gatt_sig_add_full(server,
            config->generic_service ? 0x1849 : 0x1848,
            include_count ? include_handles : NULL, include_count,
            characteristics, characteristic_count,
            &service->service_handle,
            value_handles, cccd_handles, handlers)) {
        memset(service, 0, sizeof(*service));
        return 0;
    }

    service->player_name_handle = value_handles[0];
    service->player_name_cccd_handle = cccd_handles[0];
    service->track_changed_handle = value_handles[1];
    service->changed_cccd_handle = cccd_handles[1];
    service->track_title_handle = value_handles[2];
    service->title_cccd_handle = cccd_handles[2];
    service->track_duration_handle = value_handles[3];
    service->duration_cccd_handle = cccd_handles[3];
    service->track_position_handle = value_handles[4];
    service->position_cccd_handle = cccd_handles[4];
    service->media_state_handle = value_handles[5];
    service->media_cccd_handle = cccd_handles[5];
    service->ccid_handle = value_handles[6];
    const uint16_t encrypted_cccd_permissions =
        BLE_GATT_PERM_READ_ENC | BLE_GATT_PERM_WRITE_ENC;
    for (uint8_t i = 0; i < characteristic_count; i++) {
        if (!cccd_handles[i]) continue;

        ble_gatt_attribute *cccd = gatt_server_find(server, cccd_handles[i]);

        if (cccd)
            cccd->permissions = encrypted_cccd_permissions;
    }
    uint8_t value_index = 7;

    if (option_mask & GATT_MCS_OPTION_PLAYBACK_SPEED) {
        service->playback_handle = value_handles[value_index];
        service->playback_cccd_handle = cccd_handles[value_index++];
    }

    if (option_mask & GATT_MCS_OPTION_SEEKING_SPEED) {
        service->seeking_handle = value_handles[value_index];
        service->seeking_cccd_handle = cccd_handles[value_index++];
    }

    if (option_mask & GATT_MCS_OPTION_PLAYING_ORDER) {
        service->order_handle = value_handles[value_index];
        service->order_cccd_handle = cccd_handles[value_index++];
    }

    if (option_mask & GATT_MCS_OPTION_PLAYING_ORDERS_SUPPORTED)
        service->orders_handle = value_handles[value_index++];

    if (option_mask & GATT_MCS_OPTION_CONTROL_POINT) {
        service->control_point_handle = value_handles[value_index];
        service->cp_cccd_handle = cccd_handles[value_index++];
        service->cp_opcode_mask =
            supported_mcp_opcodes;
        service->cp_opcodes_handle = value_handles[value_index];
    }

    for (uint8_t i = 0; i < 7; i++) {
        if (object_indexes[i] == 0xff)
            continue;

        service->object_handle[i] = value_handles[object_indexes[i]];
        service->object_cccd_handle[i] = cccd_handles[object_indexes[i]];
    }

    if (icon_url_index != 0xff)
        service->icon_url_handle = value_handles[icon_url_index];

    if (search_index != 0xff) {
        service->search_cp_handle = value_handles[search_index];
        service->search_cp_cccd_handle =
            cccd_handles[search_index];
    }

    return 1;
}

// Compatibility builder for mandatory MCS/GMCS metadata characteristics.
static inline int gatt_mcs_add(
    ble_gatt_server *server, ble_gatt_mcs_service *service,
    const ble_gatt_mcs_config *config
) {
    return gatt_mcs_add_full(server, service, config, NULL);
}

// Update and queue a notification for a notifiable MCS value.
static inline int gatt_mcs_notify_value(
    ble_gatt_server *server,
    const ble_gatt_mcs_service *service,
    uint16_t value_handle, const uint8_t *value, uint16_t len
) {
    if (!server || !service || service->server != server ||
        (len && !value)
    ) return 0;

    ble_gatt_attribute *attribute = gatt_server_find(server, value_handle);
    uint8_t *stored = attribute ? gatt_attribute_value(server, attribute) : NULL;

    if (!attribute || (!stored && len) || attribute->value_capacity < len)
        return 0;

    uint8_t allowed = value_handle == service->track_changed_handle ||
        value_handle == service->player_name_handle ||
        value_handle == service->track_title_handle ||
        value_handle == service->track_duration_handle ||
        value_handle == service->track_position_handle ||
        value_handle == service->media_state_handle ||
        value_handle == service->playback_handle ||
        value_handle == service->seeking_handle ||
        value_handle == service->order_handle ||
        value_handle == service->cp_opcodes_handle;

    if (!allowed) return 0;

    if (value_handle == service->track_changed_handle && len != 0)
        return 0;

    if ((value_handle == service->track_duration_handle ||
         value_handle == service->track_position_handle) && len != 4
    ) return 0;

    if ((value_handle == service->media_state_handle ||
         value_handle == service->playback_handle ||
         value_handle == service->seeking_handle ||
         value_handle == service->order_handle) && len != 1
    ) return 0;

    if (value_handle == service->cp_opcodes_handle && len != 4)
        return 0;

    if (len) memcpy(stored, value, len);

    if (attribute->flags & BLE_GATT_ATTR_VAR_LEN)
        attribute->value_len = len;
    uint16_t notify_len = len;

    if (value_handle == service->player_name_handle ||
        value_handle == service->track_title_handle) {
        if (server->mtu < 3) return 0;

        uint16_t mtu_limit = (uint16_t)(server->mtu - 3);

        if (notify_len > mtu_limit)
            notify_len = mtu_limit;
    }

    return gatt_queue_event(server, value_handle, stored, notify_len, 0);
}

// Complete a captured MCS opcode request and notify its opcode/result pair.
static inline int gatt_mcs_cp_complete(
    ble_gatt_server *server, ble_gatt_mcs_service *service,
    uint8_t result_code
) {
    if (!server || !service || service->server != server ||
        !service->cp_pending || result_code < GATT_MCS_RESULT_SUCCESS ||
        result_code > GATT_MCS_RESULT_COMMAND_CANNOT_BE_COMPLETED
    ) return 0;

    uint8_t response[2] = {service->cp_opcode, result_code};
    int queued = gatt_queue_event(server,
        service->control_point_handle, response, sizeof(response), 0);

    if (queued > 0)
        service->cp_pending = 0;
    return queued;
}

// Update an exposed MCS Object ID. An Object ID is empty or a uint48 value.
static inline int gatt_mcs_set_object_id(
    ble_gatt_server *server, const ble_gatt_mcs_service *service,
    uint8_t object, const uint8_t *value, uint16_t len
) {
    if (!server || !service || service->server != server || object >= 7 ||
        !service->object_handle[object] || (len != 0 && len != 6) ||
        (len && !value)
    ) return 0;

    ble_gatt_attribute *attribute = gatt_server_find(server,
        service->object_handle[object]);
    uint8_t *stored = attribute ? gatt_attribute_value(server, attribute) : NULL;

    if (!attribute || !stored || attribute->value_capacity < len)
        return 0;

    if (len) memcpy(stored, value, len);
    attribute->value_len = len;
    return 1;
}

// Finish the application action associated with a client Object ID write.
// Pass the resulting Object ID, or the previous value if the selection failed.
static inline int gatt_mcs_object_done(
    ble_gatt_server *server, ble_gatt_mcs_service *service,
    const uint8_t *value, uint16_t len
) {
    if (!server || !service || service->server != server ||
        !service->object_write_pending || (len && !value) ||
        (len != 0 && len != 6)
    ) return 0;

    uint8_t index = service->object_write_index;

    if (!gatt_mcs_set_object_id(server, service, index,
            value, len)
    ) return 0;

    service->object_write_pending = 0;
    service->object_write_len = 0;
    service->object_prev_len = 0;
    return 1;
}

// Update and notify a changed MCS Object ID, when its CCCD is enabled.
static inline int gatt_mcs_notify_object(
    ble_gatt_server *server, const ble_gatt_mcs_service *service,
    uint8_t object, const uint8_t *value, uint16_t len
) {
    if (!gatt_mcs_set_object_id(server, service,
            object, value, len) || !service->object_cccd_handle[object]
    ) return 0;

    ble_gatt_attribute *attribute = gatt_server_find(server,
        service->object_handle[object]);
    const uint8_t *stored = attribute ?
        gatt_attribute_value(server, attribute) : NULL;

    if (!attribute || !stored) return 0;

    return gatt_queue_event(server, service->object_handle[object],
                                   stored, len, 0);
}

// Complete a captured Search Control Point request with success or failure.
static inline int gatt_mcs_search_complete(
    ble_gatt_server *server, ble_gatt_mcs_service *service,
    uint8_t result
) {
    if (!server || !service || service->server != server ||
        !service->search_pending ||
        (result != GATT_MCS_SEARCH_SUCCESS &&
         result != GATT_MCS_SEARCH_FAILURE)
    ) return 0;

    int queued = gatt_queue_event(server,
        service->search_cp_handle, &result, 1, 0);

    if (queued > 0)
        service->search_pending = 0;
    return queued;
}

// Store the completed Search Results Object ID and notify if subscribed.
static inline int gatt_mcs_set_results_id(
    ble_gatt_server *server, const ble_gatt_mcs_service *service,
    const uint8_t *value, uint16_t len
) {
    if (!service || !service->object_handle[GATT_MCS_OBJECT_SEARCH_RESULTS] ||
        !gatt_mcs_set_object_id(server, service,
            GATT_MCS_OBJECT_SEARCH_RESULTS, value, len)
    ) return 0;

    ble_gatt_attribute *cccd = gatt_server_cccd_for(server,
        service->object_handle[GATT_MCS_OBJECT_SEARCH_RESULTS]);

    if (!cccd || !(cccd->cccd & 1))
        return 1;

    return gatt_mcs_notify_object(server, service,
        GATT_MCS_OBJECT_SEARCH_RESULTS, value, len);
}
#endif
