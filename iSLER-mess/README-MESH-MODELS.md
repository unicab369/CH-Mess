// TODO for configuration support:
// - Multiple subnets: store and route using additional NetKeys.
// - Relay and network retransmission settings.
// - Secure Network Beacon, Proxy, Friend, and Node Identity settings.
// - Heartbeat publication and subscription settings.
// - Node Reset: clear provisioning and configuration state.
// - SAR Configuration model: expose transport timing settings (separate model).

## Foundation models

Configuration support lives in `ble_mesh_4foundation.h`; remaining TODOs are listed above.

Configuration requests use `MESH_MODEL_CONFIG_SERVER`; their replies are handled
by `MESH_MODEL_CONFIG_CLIENT`. SIG means Bluetooth Special Interest Group.

| Configuration | Opcode macros | Supported |
| --- | --- | --- |
| Composition Data | `OP_CONFIG_COMPOSITION_GET`, `OP_CONFIG_COMPOSITION_STATUS` | Page 0: elements, SIG models, and supported features. |
| NetKeys | `OP_CONFIG_NETKEY_ADD`, `OP_CONFIG_NETKEY_UPDATE`, `OP_CONFIG_NETKEY_DELETE`, `OP_CONFIG_NETKEY_GET`, `OP_CONFIG_NETKEY_LIST`, `OP_CONFIG_NETKEY_STATUS` | List/update the provisioned subnet key; report duplicate adds and rejected additions/deletions. |
| Key Refresh | `OP_CONFIG_KEY_PHASE_GET`, `OP_CONFIG_KEY_PHASE_SET`, `OP_CONFIG_KEY_PHASE_STATUS` | Get the phase; select new keys with transition 2 or revoke old keys with transition 3. |
| AppKeys | `OP_CONFIG_APPKEY_ADD`, `OP_CONFIG_APPKEY_UPDATE`, `OP_CONFIG_APPKEY_DELETE`, `OP_CONFIG_APPKEY_GET`, `OP_CONFIG_APPKEY_LIST`, `OP_CONFIG_APPKEY_STATUS` | Add/update/delete/list application keys. |
| Model bindings | `OP_CONFIG_MODEL_APP_BIND`, `OP_CONFIG_MODEL_APP_UNBIND`, `OP_CONFIG_MODEL_APP_STATUS`, `OP_CONFIG_SIG_MODEL_APP_GET`, `OP_CONFIG_SIG_MODEL_APP_LIST` | Bind/unbind/list model keys. |
| Group subscriptions | `OP_CONFIG_MODEL_SUB_ADD`, `OP_CONFIG_MODEL_SUB_DELETE`, `OP_CONFIG_MODEL_SUB_OVERWRITE` | Add/delete/replace group subscriptions per model. |
| Virtual subscriptions | `OP_CONFIG_MODEL_SUB_VIRTUAL_ADD`, `OP_CONFIG_MODEL_SUB_VIRTUAL_DELETE`, `OP_CONFIG_MODEL_SUB_VIRTUAL_OVERWRITE` | Add/delete/replace Label UUID subscriptions per model. |
| Subscription lists and clearing | `OP_CONFIG_SIG_SUB_GET`, `OP_CONFIG_SIG_MODEL_SUB_LIST`, `OP_CONFIG_MODEL_SUB_DELETE_ALL`, `OP_CONFIG_MODEL_SUB_STATUS` | List subscribed addresses, clear all subscriptions, and report changes. |
| Default TTL | `OP_CONFIG_DEFAULT_TTL_GET`, `OP_CONFIG_DEFAULT_TTL_SET`, `OP_CONFIG_DEFAULT_TTL_STATUS` | Get/set; used by model helpers and replies. |
| Model publication | `OP_CONFIG_MODEL_PUB_GET`, `OP_CONFIG_MODEL_PUB_SET`, `OP_CONFIG_MODEL_PUB_VIRTUAL_SET`, `OP_CONFIG_MODEL_PUB_STATUS` | Get/set address or Label UUID, AppKey, TTL, period, and retransmissions. |

The node stores one NetKey index, installed during provisioning. Adding the same
index and current key succeeds; a different key at that index returns
`MESH_CONFIG_KEY_ALREADY_STORED`. Adding another index returns
`MESH_CONFIG_INSUFFICIENT_RESOURCES`. Deleting the only NetKey returns
`MESH_CONFIG_CANNOT_REMOVE`; deleting an absent index succeeds without changes.

To rotate keys from a controller (the [Bluetooth Mesh Key Refresh procedure](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/MshPRT_v1.1/out/en/index-en.html)):

1. Call `ble_mesh_add_or_update_net_key(dst, net_idx, new_key, 1)` on each node.
   This starts Phase 1: send with old keys, receive with old or new keys.
2. Update any AppKeys that also need rotation with
   `ble_mesh_add_or_update_app_key(dst, net_idx, app_idx, new_key, 1)`.
3. Call `ble_mesh_set_key_phase(dst, net_idx, 2)` after the selected nodes have
   their new keys. Phase 2 sends with new keys and receives with either set.
4. Call `ble_mesh_set_key_phase(dst, net_idx, 3)` after the selected nodes reach
   Phase 2. Old keys are removed and the reported phase returns to 0.

Check each reply through `BLE_MESH_CONFIG_STATUS()` before advancing. Use
`ble_mesh_get_key_phase()` to query progress. The controller must also switch its
own network keys using `ble_mesh_stage_net_key()`, `ble_mesh_stage_app_key()`, and
`ble_mesh_key_refresh_transition()` at the corresponding steps. AppKeys that were
not updated keep their values; bindings and publication settings keep their indexes.
Updates and transitions are saved before becoming active. A node provisioned
during Phase 2 reports phase 2 even though it has only the new NetKey.

| Foundation model | Opcode macros | Supported |
| --- | --- | --- |
| `MESH_MODEL_HEALTH_SERVER` | `OP_HEALTH_ATTENTION_GET`, `OP_HEALTH_ATTENTION_SET`, `OP_HEALTH_ATTENTION_SET_UNACK`, `OP_HEALTH_ATTENTION_STATUS` | Get/set the attention timer. |
| `MESH_MODEL_HEALTH_SERVER` | `OP_HEALTH_CURRENT_STATUS` | Publish an empty current-fault list. |

Call `ble_mesh_models_poll()` regularly to service publications. OnOff Servers
publish status on a state change and at the configured period. OnOff Clients use
`ble_mesh_onoff_publish()`; a configured period repeats the last published Set with
a new transaction ID. Retransmissions keep the original transaction ID. Health
Servers periodically publish an empty current-fault list. TTL 1 publications stay
on this node; TTL 0xFF uses Default TTL. Friendship credentials are unsupported.

Set `MESH_COMPANY_ID`, `MESH_PRODUCT_ID`, and `MESH_PRODUCT_VERSION` for your product;
the default company ID 0xFFFF is for internal development. Relay, Proxy, Friend,
and Low Power features are not advertised in Composition Data.
