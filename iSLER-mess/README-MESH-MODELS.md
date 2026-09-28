## Foundation models

Configuration support and its remaining TODOs live in `ble_mesh_4foundation.h`.

Configuration requests use `MESH_MODEL_CONFIG_SERVER`; their replies are handled
by `MESH_MODEL_CONFIG_CLIENT`. SIG means Bluetooth Special Interest Group.

| Configuration | Opcode macros | Supported |
| --- | --- | --- |
| Composition Data | `OP_CONFIG_COMPOSITION_GET`, `OP_CONFIG_COMPOSITION_STATUS` | Page 0: elements, SIG models, and supported features. |
| AppKeys | `OP_CONFIG_APPKEY_ADD`, `OP_CONFIG_APPKEY_UPDATE`, `OP_CONFIG_APPKEY_DELETE`, `OP_CONFIG_APPKEY_GET`, `OP_CONFIG_APPKEY_LIST`, `OP_CONFIG_APPKEY_STATUS` | Add/update/delete/list application keys. |
| Model bindings | `OP_CONFIG_MODEL_APP_BIND`, `OP_CONFIG_MODEL_APP_UNBIND`, `OP_CONFIG_MODEL_APP_STATUS`, `OP_CONFIG_SIG_MODEL_APP_GET`, `OP_CONFIG_SIG_MODEL_APP_LIST` | Bind/unbind/list model keys. |
| Group subscriptions | `OP_CONFIG_MODEL_SUB_ADD`, `OP_CONFIG_MODEL_SUB_DELETE`, `OP_CONFIG_MODEL_SUB_OVERWRITE` | Add/delete/replace group subscriptions per model. |
| Virtual subscriptions | `OP_CONFIG_MODEL_SUB_VIRTUAL_ADD`, `OP_CONFIG_MODEL_SUB_VIRTUAL_DELETE`, `OP_CONFIG_MODEL_SUB_VIRTUAL_OVERWRITE` | Add/delete/replace Label UUID subscriptions per model. |
| Subscription lists and clearing | `OP_CONFIG_SIG_SUB_GET`, `OP_CONFIG_SIG_MODEL_SUB_LIST`, `OP_CONFIG_MODEL_SUB_DELETE_ALL`, `OP_CONFIG_MODEL_SUB_STATUS` | List subscribed addresses, clear all subscriptions, and report changes. |
| Default TTL | `OP_CONFIG_DEFAULT_TTL_GET`, `OP_CONFIG_DEFAULT_TTL_SET`, `OP_CONFIG_DEFAULT_TTL_STATUS` | Get/set; used by model helpers and replies. |
| Model publication | `OP_CONFIG_MODEL_PUB_GET`, `OP_CONFIG_MODEL_PUB_SET`, `OP_CONFIG_MODEL_PUB_VIRTUAL_SET`, `OP_CONFIG_MODEL_PUB_STATUS` | Get/set address or Label UUID, AppKey, TTL, period, and retransmissions. |

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
