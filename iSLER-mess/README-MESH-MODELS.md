// TODO for foundation support:
// - Relay, Proxy, Friend, and Node Identity feature implementations.
// - Mesh Private Beacon support.

## Foundation models

Configuration and Health support live in `ble_mesh_4foundation.h`; remaining TODOs are listed above.

Configuration requests use `MESH_MODEL_CONFIG_SERVER`; their replies are handled
by `MESH_MODEL_CONFIG_CLIENT`. SIG means Bluetooth Special Interest Group.

| Configuration | Opcode macros | Supported |
| --- | --- | --- |
| Composition Data | `OP_CONFIG_COMPOSITION_GET`, `OP_CONFIG_COMPOSITION_STATUS` | Page 0: elements, SIG models, and supported features. |
| NetKeys | `OP_CONFIG_NETKEY_ADD`, `OP_CONFIG_NETKEY_UPDATE`, `OP_CONFIG_NETKEY_DELETE`, `OP_CONFIG_NETKEY_GET`, `OP_CONFIG_NETKEY_LIST`, `OP_CONFIG_NETKEY_STATUS` | Add, list, update, and delete subnet keys; transmit and receive on the matching subnet. |
| Key Refresh | `OP_CONFIG_KEY_PHASE_GET`, `OP_CONFIG_KEY_PHASE_SET`, `OP_CONFIG_KEY_PHASE_STATUS` | Get the phase; select new keys with transition 2 or revoke old keys with transition 3. |
| AppKeys | `OP_CONFIG_APPKEY_ADD`, `OP_CONFIG_APPKEY_UPDATE`, `OP_CONFIG_APPKEY_DELETE`, `OP_CONFIG_APPKEY_GET`, `OP_CONFIG_APPKEY_LIST`, `OP_CONFIG_APPKEY_STATUS` | Add/update/delete/list application keys. |
| Model bindings | `OP_CONFIG_MODEL_APP_BIND`, `OP_CONFIG_MODEL_APP_UNBIND`, `OP_CONFIG_MODEL_APP_STATUS`, `OP_CONFIG_SIG_MODEL_APP_GET`, `OP_CONFIG_SIG_MODEL_APP_LIST` | Bind/unbind/list model keys. |
| Group subscriptions | `OP_CONFIG_MODEL_SUB_ADD`, `OP_CONFIG_MODEL_SUB_DELETE`, `OP_CONFIG_MODEL_SUB_OVERWRITE` | Add/delete/replace group subscriptions per model. |
| Virtual subscriptions | `OP_CONFIG_MODEL_SUB_VIRTUAL_ADD`, `OP_CONFIG_MODEL_SUB_VIRTUAL_DELETE`, `OP_CONFIG_MODEL_SUB_VIRTUAL_OVERWRITE` | Add/delete/replace Label UUID subscriptions per model. |
| Subscription lists and clearing | `OP_CONFIG_SIG_SUB_GET`, `OP_CONFIG_SIG_MODEL_SUB_LIST`, `OP_CONFIG_MODEL_SUB_DELETE_ALL`, `OP_CONFIG_MODEL_SUB_STATUS` | List subscribed addresses, clear all subscriptions, and report changes. |
| Secure Network Beacon | `OP_CONFIG_BEACON_GET`, `OP_CONFIG_BEACON_SET`, `OP_CONFIG_BEACON_STATUS` | Get/set automatic beacon broadcasts. |
| Network Transmit | `OP_CONFIG_NET_TRANSMIT_GET`, `OP_CONFIG_NET_TRANSMIT_SET`, `OP_CONFIG_NET_TRANSMIT_STATUS` | Get/set packet repetitions and their interval. |
| SAR Transmitter | `OP_CONFIG_SAR_TRANSMITTER_GET`, `OP_CONFIG_SAR_TRANSMITTER_SET`, `OP_CONFIG_SAR_TRANSMITTER_STATUS` | Get/set segment interval and segmented-message retransmission settings. |
| SAR Receiver | `OP_CONFIG_SAR_RECEIVER_GET`, `OP_CONFIG_SAR_RECEIVER_SET`, `OP_CONFIG_SAR_RECEIVER_STATUS` | Get/set segment threshold, ACK timing/retries, and incomplete-message discard timeout. |
| Relay | `OP_CONFIG_RELAY_GET`, `OP_CONFIG_RELAY_SET`, `OP_CONFIG_RELAY_STATUS` | Report Not Supported (2); Relay Retransmit is 0. |
| GATT Proxy | `OP_CONFIG_PROXY_GET`, `OP_CONFIG_PROXY_SET`, `OP_CONFIG_PROXY_STATUS` | Report Not Supported (2). |
| Friend | `OP_CONFIG_FRIEND_GET`, `OP_CONFIG_FRIEND_SET`, `OP_CONFIG_FRIEND_STATUS` | Report Not Supported (2). |
| Node Identity | `OP_CONFIG_NODE_IDENTITY_GET`, `OP_CONFIG_NODE_IDENTITY_SET`, `OP_CONFIG_NODE_IDENTITY_STATUS` | Report Not Supported (2); setting it returns Feature Not Supported. |
| Node Reset | `OP_CONFIG_NODE_RESET`, `OP_CONFIG_NODE_RESET_STATUS` | Reply, then clear provisioning, keys, configuration, and queued traffic. |
| Heartbeat publication | `OP_CONFIG_HEARTBEAT_PUB_GET`, `OP_CONFIG_HEARTBEAT_PUB_SET`, `OP_CONFIG_HEARTBEAT_PUB_STATUS` | Configure periodic Control messages using the selected NetKey. |
| Heartbeat subscription | `OP_CONFIG_HEARTBEAT_SUB_GET`, `OP_CONFIG_HEARTBEAT_SUB_SET`, `OP_CONFIG_HEARTBEAT_SUB_STATUS` | Monitor one source/destination pair, received count, and minimum/maximum hops. |
| Default TTL | `OP_CONFIG_DEFAULT_TTL_GET`, `OP_CONFIG_DEFAULT_TTL_SET`, `OP_CONFIG_DEFAULT_TTL_STATUS` | Get/set; used by model helpers and replies. |
| Model publication | `OP_CONFIG_MODEL_PUB_GET`, `OP_CONFIG_MODEL_PUB_SET`, `OP_CONFIG_MODEL_PUB_VIRTUAL_SET`, `OP_CONFIG_MODEL_PUB_STATUS` | Get/set address or Label UUID, AppKey, TTL, period, and retransmissions. |

`mesh_set_beacon(dst, enabled)` controls Secure Network Beacon broadcasts;
they are enabled after provisioning. The network poll schedules them with a
10–600 second interval, adjusted using authenticated subnet beacons heard over
two rolling 10-second windows. Disabling broadcasts still allows receiving
beacons for Key Refresh and IV Update. Manual beacon sends also obey this setting.

`mesh_set_net_transmit(dst, count, interval_steps)` sets 0–7 extra sends and
0–31 interval steps. Each packet is sent `count + 1` times, with
`(interval_steps + 1) * 10` milliseconds plus 0–10 milliseconds of jitter between
completed advertising events. The default count is 0 (one send). Repetitions keep
the same encrypted Network PDU and sequence number; beacons and provisioning
packets do not use this setting. The existing eight-slot advertising queue holds
the repetitions and rejects new packets when full. New settings apply to newly
queued packets. Credential changes discard queued network packets that use old
credentials. Stored state version 16 requires reprovisioning older records
when using the default four-subnet limit.

The node supports up to `MESH_MAX_SUBNETS` NetKeys, with a default of four.
Set this compile-time limit from 2 through 16 to trade RAM for subnet capacity;
the state version changes with this value, so changing it requires reprovisioning.
AppKeys retain their owning NetKey index; AppKey traffic uses that subnet, while
Device Key configuration requests can select a subnet through their NetKey index.

`mesh_set_heartbeat_pub(dst, &pub)` configures a `mesh_heartbeat_publication`:
destination, NetKey index, count log, period log, TTL, and feature-change triggers.
Logs 1–17 represent powers of two (`2^(log - 1)`); 0 disables periodic sends.
Count log 17 selects 65,534 sends; 255 sends indefinitely. The first Heartbeat is
queued immediately on the next network poll. Feature-change triggers are zero
because Relay, Proxy, Friend, and LPN are unsupported. Heartbeats are unsegmented
Control messages and use Network Transmit repetitions.

`mesh_set_heartbeat_sub(dst, src, address, period_log)` monitors one source
at the node's primary address or a group address. A zero source/destination clears
the subscription; a zero period stops monitoring. Status reports the remaining
period, received count, and minimum/maximum hops (`initial TTL - received TTL + 1`).
Use `mesh_get_heartbeat_pub()` and `mesh_get_heartbeat_sub()` to query them.
Publication settings are saved; only indefinite publications resume after reboot.
Finite remaining counts and subscription sessions are RAM data and restart disabled.

`mesh_reset_node(dst)` sends an authenticated Node Reset request. The node
finishes advertising Reset Status before writing an empty flash record, erasing
the old copy, and clearing runtime state. Flash failures leave reset pending for
retry. Reset is rejected while PB-ADV is active or when provisioner node records
are stored. After reset, the application can call `provisionee_start()` and resume
`provisionee_poll()` to accept provisioning again.

Use `mesh_get_relay()`, `mesh_get_proxy()`, `mesh_get_friend()`, and
`mesh_get_node_identity()` to query capabilities. Relay, Proxy, and Friend Set
requests report Not Supported without changing state. Node Identity queries use
a NetKey index; an unknown index returns Invalid NetKey.

Provisioning installs the primary NetKey. Configuration can add further NetKeys
up to `MESH_MAX_SUBNETS`; adding an existing index with the same key succeeds,
while a different key at that index returns `MESH_CONFIG_KEY_ALREADY_STORED`.
Deleting the final NetKey returns `MESH_CONFIG_CANNOT_REMOVE`; deleting an absent
index succeeds without changes. Deleting the primary promotes an installed subnet.

To rotate keys from a controller (the [Bluetooth Mesh Key Refresh procedure](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/MshPRT_v1.1/out/en/index-en.html)):

1. Call `mesh_netkey_add_or_update(dst, net_idx, new_key, 1)` on each node.
   This starts Phase 1: send with old keys, receive with old or new keys.
2. Update any AppKeys that also need rotation with
   `mesh_add_or_update_app_key(dst, net_idx, app_idx, new_key, 1)`.
3. Call `mesh_netkey_set_phase(dst, net_idx, 2)` after the selected nodes have
   their new keys. Phase 2 sends with new keys and receives with either set.
4. Call `mesh_netkey_set_phase(dst, net_idx, 3)` after the selected nodes reach
   Phase 2. Old keys are removed and the reported phase returns to 0.

Check each reply through `BLE_MESH_CONFIG_STATUS()` before advancing. Use
`mesh_netkey_get_phase()` to query progress. The controller must also switch its
own network keys with the same sequence, using `mesh_stage_net_key(net_idx, key)`,
`mesh_stage_app_key()`, and the Config Key Phase Set operation addressed to itself.
AppKeys that were
not updated keep their values; bindings and publication settings keep their indexes.
Updates and transitions are saved before becoming active. A node provisioned
during Phase 2 reports phase 2 even though it has only the new NetKey.

| Foundation model | Opcode macros | Supported |
| --- | --- | --- |
| `MESH_MODEL_HEALTH_SERVER` | `OP_HEALTH_ATTENTION_GET`, `OP_HEALTH_ATTENTION_SET`, `OP_HEALTH_ATTENTION_SET_UNACK`, `OP_HEALTH_ATTENTION_STATUS` | Get/set the attention timer. |
| `MESH_MODEL_HEALTH_SERVER` | `OP_HEALTH_CURRENT_STATUS` | Publish current faults and the last test ID. |
| `MESH_MODEL_HEALTH_SERVER` | `OP_HEALTH_FAULT_GET`, `OP_HEALTH_FAULT_CLEAR`, `OP_HEALTH_FAULT_CLEAR_UNACK`, `OP_HEALTH_FAULT_STATUS` | Read or clear recorded fault history; clearing does not remove active faults. |
| `MESH_MODEL_HEALTH_SERVER` | `OP_HEALTH_FAULT_TEST`, `OP_HEALTH_FAULT_TEST_UNACK` | Run a supported application self-test and update faults. |
| `MESH_MODEL_HEALTH_SERVER` | `OP_HEALTH_PERIOD_GET`, `OP_HEALTH_PERIOD_SET`, `OP_HEALTH_PERIOD_SET_UNACK`, `OP_HEALTH_PERIOD_STATUS` | Get/set the saved Fast Period Divisor (0–15), per element. |
| `MESH_MODEL_HEALTH_CLIENT` | `OP_HEALTH_FAULT_GET`, `OP_HEALTH_FAULT_CLEAR`, `OP_HEALTH_FAULT_CLEAR_UNACK`, `OP_HEALTH_FAULT_TEST`, `OP_HEALTH_FAULT_TEST_UNACK` | Query/clear remote fault history or run a remote self-test. |
| `MESH_MODEL_HEALTH_CLIENT` | `OP_HEALTH_PERIOD_GET`, `OP_HEALTH_PERIOD_SET`, `OP_HEALTH_PERIOD_SET_UNACK`, `OP_HEALTH_ATTENTION_GET`, `OP_HEALTH_ATTENTION_SET`, `OP_HEALTH_ATTENTION_SET_UNACK` | Query/set remote publication divisor and attention timer. |
| `MESH_MODEL_HEALTH_CLIENT` | `OP_HEALTH_CURRENT_STATUS`, `OP_HEALTH_FAULT_STATUS`, `OP_HEALTH_PERIOD_STATUS`, `OP_HEALTH_ATTENTION_STATUS` | Deliver replies and subscribed current faults to the application. |

Use `mesh_health_fault_get/clear/test()`, `mesh_health_period_get/set()`,
and `mesh_health_attention_get/set()` with a local element, destination, and
bound AppKey index. Fault requests also take a Company ID; Test takes a Test ID.
Clear, Test, and Set take `acknowledged` (1 requests a reply, 0 does not).
Use the matching `_virtual` helper with a 16-byte Label UUID to send a request
to a virtual address. Requests can target unicast, group, or virtual addresses.
Set `dst` to 0 to send through the Health Client publication configured with
`mesh_set_publication()`. The helper AppKey index must match that publication.
Health Client publication retransmits a request but does not send periodic requests;
the model configuration Publish Period is ignored for this client model.

`BLE_MESH_HEALTH_STATUS(element, src, opcode, params, len)` receives validated
replies and published current faults. Fault parameters are Test ID (1 byte),
Company ID (2 bytes, little endian), then fault codes; Period and Attention
contain one byte. Copy parameters during the callback if retaining them.
Health Clients appear on every element in Composition Data; bindings and
subscriptions are saved per element. Group/virtual reports reach only subscribed
Health Clients with the receiving AppKey bound. Remote fault lists are not limited
to the local server's fault capacity. The default callback in `ble_mesh.h` is empty.

`mesh_health_faults(element, test_id, faults, count)` reports the element's
current faults. Pass `NULL, 0` when recovered. Nonzero fault codes are deduplicated
and added to registered history. The default capacity is five distinct current
faults and five registered faults per element; override `MESH_HEALTH_MAX_FAULTS`
(1–32) before including the headers. Invalid input or full history returns 0
without changing state. Current faults and history are RAM data and clear on
reboot or Node Reset. Health Fault Clear clears only history.

`BLE_MESH_HEALTH_TEST(element, test_id, faults, &count)` is the synchronous
application self-test interface: count is buffer capacity on input and result
length on output. Return 1 after a supported test, or 0 for an unsupported test
or failure. The default in `ble_mesh.h` supports standard test 0 by returning the
latest application-reported faults; add hardware diagnostics or vendor tests
there. Server requests for another Company ID or an unsupported test are ignored.

Health requests use a bound AppKey. Fault changes publish Current Status through
the configured Health publication, including when the periodic interval is zero.
While faults are active, periodic reports use `Publish Period / 2^divisor`, with
a 100 ms minimum; recovery restores the normal period. The divisor is saved to
flash. Publication retransmissions retain the original fault snapshot.

Call `mesh_models_poll()` regularly to service publications. OnOff Servers
publish status on a state change and at the configured period. OnOff Clients use
`mesh_onoff_publish()`; a configured period repeats the last published Set with
a new transaction ID. Retransmissions keep the original transaction ID. Health
Servers periodically publish their current-fault list. TTL 1 publications stay
on this node; TTL 0xFF uses Default TTL. Friendship credentials are unsupported.

Set `MESH_COMPANY_ID`, `MESH_PRODUCT_ID`, and `MESH_PRODUCT_VERSION` for your product;
the default company ID 0xFFFF is for internal development. Relay, Proxy, Friend,
and Low Power features are not advertised in Composition Data.
