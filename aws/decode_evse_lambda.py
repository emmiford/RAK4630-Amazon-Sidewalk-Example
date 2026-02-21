"""
Lambda function to decode EVSE Sidewalk sensor data.

Supports five payload formats:
1. v0x0A raw format (15 bytes): Magic 0xE5, J1772, voltage, current, flags+control,
   timestamp, transition reason, app build version, platform build version
2. v0x09 raw format (13 bytes): Same as v0x0A without build versions
3. v0x08 raw format (12 bytes): Same as v0x09 without transition reason
   (bit 0 of flags reserved — no heat flag)
4. v0x07 raw format (12 bytes): Same layout as v0x08, includes heat flag in bit 0
5. v0x06 raw format (8 bytes): Magic 0xE5, J1772, voltage, current, thermostat+faults
6. Legacy sid_demo format: Wrapped with demo protocol headers

Extracts:
- J1772 pilot state
- Pilot voltage (mV)
- Current draw (mA)
- Thermostat input bits
- Charge control flags (v0x07+)
- Device-side timestamp (v0x07+)
"""

import base64
import json
import os
import time
from datetime import datetime
from decimal import Decimal
from zoneinfo import ZoneInfo

import boto3

import device_registry

dynamodb = boto3.resource('dynamodb')
lambda_client = boto3.client('lambda')
table_name = os.environ.get('DYNAMODB_TABLE', 'sidewalk-v1-device_events_v2')
ota_lambda_name = os.environ.get('OTA_LAMBDA_NAME', 'ota-sender')
scheduler_lambda_name = os.environ.get('SCHEDULER_LAMBDA_NAME', 'charge-scheduler')
registry_table_name = os.environ.get('DEVICE_REGISTRY_TABLE', 'sidecharge-device-registry')
table = dynamodb.Table(table_name)
registry_table = dynamodb.Table(registry_table_name)

from protocol_constants import OTA_CMD_TYPE, OTA_SUB_ACK, OTA_SUB_COMPLETE, OTA_SUB_STATUS, SIDECHARGE_EPOCH_OFFSET

# TIME_SYNC constants
TIME_SYNC_CMD_TYPE = 0x30
TIME_SYNC_INTERVAL_S = 86400  # Re-sync daily

from sidewalk_utils import send_sidewalk_msg, get_device_id  # noqa: E402

# Scheduler divergence detection (TASK-071)
DIVERGENCE_MAX_RETRIES = 3
DIVERGENCE_GRACE_S = 60  # Skip check within 60s of last command send

# Charge Now opt-out (TASK-064 / ADR-003)
MT = ZoneInfo("America/Denver")
CHARGE_NOW_DEFAULT_DURATION_S = 14400  # 4 hours fallback when not in TOU peak

# EVSE payload magic byte and versions
EVSE_MAGIC = 0xE5
EVSE_PAYLOAD_SIZE_V06 = 8
EVSE_PAYLOAD_SIZE_V07 = 12
EVSE_PAYLOAD_SIZE_V09 = 13
EVSE_PAYLOAD_SIZE_V0A = 15

# Diagnostics payload (TASK-029 Tier 2)
DIAG_MAGIC = 0xE6
DIAG_PAYLOAD_SIZE = 15

# Legacy payload type
LEGACY_EVSE_TYPE = 0x01




# Transition reason mapping (matches charge_control.h)
TRANSITION_REASONS = {
    0x00: 'none',
    0x01: 'cloud_cmd',
    0x02: 'delay_window',
    0x03: 'charge_now',
    0x04: 'auto_resume',
    0x05: 'manual',
}

# J1772 state mapping (matches firmware enum in evse_sensors.h)
J1772_STATES = {
    0: 'A',  # No vehicle (+12V)
    1: 'B',  # Vehicle connected, not ready (+9V)
    2: 'C',  # Vehicle ready, charging (+6V)
    3: 'D',  # Vehicle ready, ventilation required (+3V)
    4: 'E',  # Error - short circuit (0V)
    5: 'F',  # Error - no pilot (-12V)
    6: 'UNKNOWN',
}


def _build_time_sync_bytes(sc_epoch, watermark):
    """Build a 9-byte TIME_SYNC downlink payload.

    Format: [0x30] [epoch LE 4B] [watermark LE 4B]
    """
    payload = bytearray(9)
    payload[0] = TIME_SYNC_CMD_TYPE
    payload[1] = sc_epoch & 0xFF
    payload[2] = (sc_epoch >> 8) & 0xFF
    payload[3] = (sc_epoch >> 16) & 0xFF
    payload[4] = (sc_epoch >> 24) & 0xFF
    payload[5] = watermark & 0xFF
    payload[6] = (watermark >> 8) & 0xFF
    payload[7] = (watermark >> 16) & 0xFF
    payload[8] = (watermark >> 24) & 0xFF
    return bytes(payload)


def maybe_send_time_sync(device_id, device_timestamp=None):
    """Send TIME_SYNC if no sentinel exists, sentinel is >24h old,
    or device reports timestamp=0 (lost sync after reboot/reflash)."""
    device_needs_sync = device_timestamp is not None and device_timestamp == 0
    if not device_needs_sync:
        try:
            resp = table.get_item(Key={
                'device_id': device_id,
                'timestamp': -2,
            })
            item = resp.get('Item')
            if item:
                last_sync = item.get('last_sync_unix', 0)
                if (int(time.time()) - last_sync) < TIME_SYNC_INTERVAL_S:
                    return  # Recently synced, skip
        except Exception as e:
            print(f"TIME_SYNC sentinel read error: {e}")
    else:
        print("Device reports ts=0 (unsynced), forcing TIME_SYNC")

    # Build and send TIME_SYNC
    now_unix = int(time.time())
    sc_epoch = now_unix - SIDECHARGE_EPOCH_OFFSET
    watermark = sc_epoch  # ACK watermark = current time (all data received so far)
    payload = _build_time_sync_bytes(sc_epoch, watermark)
    send_sidewalk_msg(payload)
    print(f"Sent TIME_SYNC: epoch={sc_epoch}, watermark={watermark}")

    # Write sentinel
    table.put_item(Item={
        'device_id': device_id,
        'timestamp': -2,
        'event_type': 'time_sync_state',
        'last_sync_unix': now_unix,
        'last_sync_epoch': sc_epoch,
    })


def check_scheduler_divergence(device_id, charge_allowed):
    """Compare device's charge_allowed against scheduler sentinel.

    If the sentinel's last_command disagrees with the device state,
    re-invoke the scheduler to re-send. A separate divergence tracker
    (timestamp=-3) caps retries at DIVERGENCE_MAX_RETRIES.
    """
    now_unix = int(time.time())

    # Read scheduler sentinel (timestamp=0)
    try:
        resp = table.get_item(Key={'device_id': device_id, 'timestamp': 0})
    except Exception as e:
        print(f"Divergence check: sentinel read error: {e}")
        return
    sentinel = resp.get('Item')
    if not sentinel:
        return  # No scheduler state yet

    last_command = sentinel.get('last_command')
    if last_command not in ('delay_window', 'allow'):
        return  # Only check when cloud explicitly sent a command

    # Grace period: skip if command was sent very recently
    sent_unix = int(sentinel.get('sent_unix', 0))
    if sent_unix and (now_unix - sent_unix) < DIVERGENCE_GRACE_S:
        return

    # Determine expected device state from sentinel
    # delay_window → device should have charge_allowed=False
    # allow → device should have charge_allowed=True
    expected_allowed = (last_command == 'allow')

    if charge_allowed == expected_allowed:
        # No divergence — reset tracker if it was active
        try:
            resp = table.get_item(Key={'device_id': device_id, 'timestamp': -3})
            tracker = resp.get('Item')
            if tracker and int(tracker.get('retry_count', 0)) > 0:
                table.put_item(Item={
                    'device_id': device_id,
                    'timestamp': -3,
                    'event_type': 'divergence_state',
                    'retry_count': 0,
                })
                print("Divergence resolved, tracker reset")
        except Exception as e:
            print(f"Divergence tracker reset error: {e}")
        return

    # Divergence detected
    print(f"DIVERGENCE: sentinel={last_command}, device charge_allowed={charge_allowed}")

    # Read divergence tracker
    try:
        resp = table.get_item(Key={'device_id': device_id, 'timestamp': -3})
        tracker = resp.get('Item')
        retry_count = int(tracker.get('retry_count', 0)) if tracker else 0
    except Exception as e:
        print(f"Divergence tracker read error: {e}")
        retry_count = 0

    if retry_count >= DIVERGENCE_MAX_RETRIES:
        print(f"DIVERGENCE_RETRIES_EXHAUSTED: {retry_count} retries, "
              f"sentinel={last_command}, device={charge_allowed}")
        return

    # Increment tracker and re-invoke scheduler
    retry_count += 1
    try:
        table.put_item(Item={
            'device_id': device_id,
            'timestamp': -3,
            'event_type': 'divergence_state',
            'retry_count': retry_count,
            'last_divergence_unix': now_unix,
            'sentinel_command': last_command,
            'device_charge_allowed': charge_allowed,
        })
    except Exception as e:
        print(f"Divergence tracker write error: {e}")
        return

    # Re-invoke scheduler with force_resend flag
    try:
        lambda_client.invoke(
            FunctionName=scheduler_lambda_name,
            InvocationType='Event',  # async
            Payload=json.dumps({'force_resend': True}),
        )
        print(f"Divergence retry {retry_count}/{DIVERGENCE_MAX_RETRIES}: "
              f"re-invoked scheduler")
    except Exception as e:
        print(f"Divergence: scheduler invoke failed: {e}")


def handle_charge_now_override(device_id):
    """Write charge_now_override_until to scheduler sentinel (ADR-003).

    Called when FLAG_CHARGE_NOW=1 in an uplink. Sets the override to the
    end of the current TOU peak window (9 PM MT for Xcel Colorado), or
    now + 4 hours if no peak window is active.
    """
    now_unix = int(time.time())
    now_mt = datetime.fromtimestamp(now_unix, MT)

    # If currently in TOU peak (weekdays 5-9 PM MT), override until 9 PM
    if now_mt.weekday() < 5 and 17 <= now_mt.hour < 21:
        peak_end = now_mt.replace(hour=21, minute=0, second=0, microsecond=0)
        override_until = int(peak_end.timestamp())
    else:
        override_until = now_unix + CHARGE_NOW_DEFAULT_DURATION_S

    # Update sentinel — uses update_item to avoid overwriting scheduler fields
    table.update_item(
        Key={'device_id': device_id, 'timestamp': 0},
        UpdateExpression='SET charge_now_override_until = :val',
        ExpressionAttributeValues={':val': override_until},
    )
    override_mt = datetime.fromtimestamp(override_until, MT)
    print(f"Charge Now opt-out: set override_until={override_until} "
          f"({override_mt.isoformat()})")


def decode_raw_evse_payload(raw_bytes):
    """
    Decode raw EVSE payload (v0x06 = 8 bytes, v0x07/v0x08 = 12 bytes).

    v0x06 format (8 bytes):
      Byte 0: Magic (0xE5)
      Byte 1: Version (0x06)
      Byte 2: J1772 state (0-6)
      Byte 3-4: Pilot voltage mV (little-endian)
      Byte 5-6: Current mA (little-endian)
      Byte 7: Flags — thermostat (bits 0-1) + faults (bits 4-7)

    v0x07 format (12 bytes):
      Bytes 0-7: Same as v0x06
      Byte 7: Flags — thermostat (bits 0-1) + charge_allowed (bit 2)
               + charge_now (bit 3) + faults (bits 4-7)
      Byte 8-11: SideCharge epoch timestamp (LE uint32, seconds since 2026-01-01)

    v0x08 format (12 bytes):
      Same byte layout as v0x07.
      Bit 0 of flags is reserved (always 0) — heat call removed in v1.0.
    """
    if len(raw_bytes) < EVSE_PAYLOAD_SIZE_V06:
        return None

    magic = raw_bytes[0]
    version = raw_bytes[1]

    if magic != EVSE_MAGIC:
        return None

    j1772_state = raw_bytes[2]
    pilot_voltage = int.from_bytes(raw_bytes[3:5], 'little')
    current_ma = int.from_bytes(raw_bytes[5:7], 'little')
    flags_byte = raw_bytes[7]

    # Sanity check values
    if j1772_state > 6:
        return None
    if pilot_voltage > 15000 or current_ma > 100000:
        return None

    result = {
        'payload_type': 'evse',
        'format': 'raw_v1',
        'version': version,
        'j1772_state_code': j1772_state,
        'j1772_state': J1772_STATES.get(j1772_state, 'UNKNOWN'),
        'pilot_voltage_mv': pilot_voltage,
        'current_ma': current_ma,
        'thermostat_cool': bool(flags_byte & 0x02),
        'charge_allowed': bool(flags_byte & 0x04),
        'charge_now': bool(flags_byte & 0x08),
        'fault_sensor': bool(flags_byte & 0x10),
        'fault_clamp_mismatch': bool(flags_byte & 0x20),
        'fault_interlock': bool(flags_byte & 0x40),
        'fault_selftest_fail': bool(flags_byte & 0x80),
    }

    # v0x07 and earlier: include heat flag (bit 0 was thermostat heat)
    if version < 0x08:
        result['thermostat_bits'] = flags_byte & 0x03
        result['thermostat_heat'] = bool(flags_byte & 0x01)
    else:
        # v0x08+: bit 0 reserved (always 0), only cool in bit 1
        result['thermostat_bits'] = flags_byte & 0x02

    # v0x07+: 4-byte timestamp at bytes 8-11
    if len(raw_bytes) >= EVSE_PAYLOAD_SIZE_V07:
        sc_epoch = int.from_bytes(raw_bytes[8:12], 'little')
        result['device_timestamp_epoch'] = sc_epoch
        if sc_epoch > 0:
            result['device_timestamp_unix'] = sc_epoch + SIDECHARGE_EPOCH_OFFSET
        else:
            result['device_timestamp_unix'] = None  # Not yet synced

    # v0x09+: transition reason byte at byte 12
    if len(raw_bytes) >= EVSE_PAYLOAD_SIZE_V09 and version >= 0x09:
        reason_code = raw_bytes[12]
        result['transition_reason_code'] = reason_code
        result['transition_reason'] = TRANSITION_REASONS.get(reason_code, f'unknown_{reason_code}')

    # v0x0A+: app and platform build versions at bytes 13-14
    if len(raw_bytes) >= EVSE_PAYLOAD_SIZE_V0A and version >= 0x0A:
        result['app_build_version'] = raw_bytes[13]
        result['platform_build_version'] = raw_bytes[14]

    return result


def decode_legacy_sid_demo_payload(raw_bytes):
    """
    Decode legacy sid_demo format payload.

    The payload structure:
    - Wrapped in sid_demo format (variable header bytes)
    - Inner payload is evse_payload_t:
      - payload_type: 1 byte (0x01 = EVSE)
      - j1772_state: 1 byte
      - pilot_voltage: 2 bytes (little-endian, mV)
      - current_ma: 2 bytes (little-endian, mA)
      - thermostat_bits: 1 byte
    """
    if len(raw_bytes) < 7:
        return None

    # Try to find EVSE payload (type 0x01) at various offsets
    for offset in range(len(raw_bytes) - 6):
        payload_type = raw_bytes[offset]
        if payload_type == LEGACY_EVSE_TYPE:
            j1772_state = raw_bytes[offset + 1]
            if j1772_state <= 6:  # Valid J1772 state
                pilot_voltage = int.from_bytes(raw_bytes[offset + 2:offset + 4], 'little')
                current_ma = int.from_bytes(raw_bytes[offset + 4:offset + 6], 'little')
                thermostat_bits = raw_bytes[offset + 6] if offset + 6 < len(raw_bytes) else 0

                # Sanity check values
                if 0 <= pilot_voltage <= 15000 and 0 <= current_ma <= 100000:
                    return {
                        'payload_type': 'evse',
                        'format': 'sid_demo_legacy',
                        'j1772_state_code': j1772_state,
                        'j1772_state': J1772_STATES.get(j1772_state, 'UNKNOWN'),
                        'pilot_voltage_mv': pilot_voltage,
                        'current_ma': current_ma,
                        'thermostat_bits': thermostat_bits,
                        'thermostat_heat': bool(thermostat_bits & 0x01),
                        'thermostat_cool': bool(thermostat_bits & 0x02),
                    }

    return None


def decode_ota_uplink(raw_bytes):
    """
    Decode OTA uplink messages (cmd type 0x20).

    ACK (0x80):      status(1) next_chunk(2) chunks_received(2) = 7B total
    COMPLETE (0x81): result(1) crc32_calc(4) = 7B total
    STATUS (0x82):   phase(1) chunks_rcvd(2) total_chunks(2) app_ver(4) = 11B total
    """
    if len(raw_bytes) < 2 or raw_bytes[0] != OTA_CMD_TYPE:
        return None

    subtype = raw_bytes[1]

    if subtype == OTA_SUB_ACK and len(raw_bytes) >= 7:
        return {
            'payload_type': 'ota',
            'ota_type': 'ack',
            'status': raw_bytes[2],
            'next_chunk': int.from_bytes(raw_bytes[3:5], 'little'),
            'chunks_received': int.from_bytes(raw_bytes[5:7], 'little'),
        }

    if subtype == OTA_SUB_COMPLETE and len(raw_bytes) >= 7:
        return {
            'payload_type': 'ota',
            'ota_type': 'complete',
            'result': raw_bytes[2],
            'crc32_calc': f"0x{int.from_bytes(raw_bytes[3:7], 'little'):08x}",
        }

    if subtype == OTA_SUB_STATUS and len(raw_bytes) >= 11:
        return {
            'payload_type': 'ota',
            'ota_type': 'status',
            'phase': raw_bytes[2],
            'chunks_received': int.from_bytes(raw_bytes[3:5], 'little'),
            'total_chunks': int.from_bytes(raw_bytes[5:7], 'little'),
            'app_version': int.from_bytes(raw_bytes[7:11], 'little'),
        }

    return {
        'payload_type': 'ota',
        'ota_type': 'unknown',
        'subtype': subtype,
        'raw_hex': raw_bytes.hex(),
    }


def decode_diag_payload(raw_bytes):
    """
    Decode extended diagnostics payload (magic 0xE6, 14 bytes).

    Sent by the device in response to a 0x40 diagnostics request.
    See TDD §3.5.
    """
    if len(raw_bytes) < DIAG_PAYLOAD_SIZE:
        return None

    if raw_bytes[0] != DIAG_MAGIC:
        return None

    diag_version = raw_bytes[1]
    app_version = int.from_bytes(raw_bytes[2:4], 'little')
    uptime_s = int.from_bytes(raw_bytes[4:8], 'little')
    boot_count = int.from_bytes(raw_bytes[8:10], 'little')
    last_error = raw_bytes[10]
    state_flags = raw_bytes[11]
    event_buf_pending = raw_bytes[12]
    app_build_version = raw_bytes[13] if len(raw_bytes) > 13 else 0
    platform_build_version = raw_bytes[14] if len(raw_bytes) > 14 else 0

    # Map error code to name
    error_names = {0: 'none', 1: 'sensor', 2: 'clamp', 3: 'interlock', 4: 'selftest'}
    error_name = error_names.get(last_error, f'unknown_{last_error}')

    return {
        'payload_type': 'diagnostics',
        'diag_version': diag_version,
        'app_version': app_version,
        'app_build_version': app_build_version,
        'platform_build_version': platform_build_version,
        'uptime_seconds': uptime_s,
        'boot_count': boot_count,
        'last_error_code': last_error,
        'last_error_name': error_name,
        'state_flags': state_flags,
        'sidewalk_ready': bool(state_flags & 0x01),
        'charge_allowed': bool(state_flags & 0x02),
        'charge_now': bool(state_flags & 0x04),
        'interlock_active': bool(state_flags & 0x08),
        'selftest_pass': bool(state_flags & 0x10),
        'ota_in_progress': bool(state_flags & 0x20),
        'time_synced': bool(state_flags & 0x40),
        'event_buffer_pending': event_buf_pending,
    }


def decode_payload(raw_payload_b64):
    """
    Decode EVSE payload from base64-encoded Sidewalk message.

    Tries new raw format first, falls back to legacy sid_demo format.
    """
    try:
        # Decode base64 - payload may be ASCII hex encoded after base64
        decoded_b64 = base64.b64decode(raw_payload_b64)

        # Try to interpret as ASCII hex first (legacy encoding)
        try:
            ascii_hex = decoded_b64.decode('ascii')
            raw_bytes = bytes.fromhex(ascii_hex)
        except (UnicodeDecodeError, ValueError):
            # Not ASCII hex, use raw bytes directly
            raw_bytes = decoded_b64

        print(f"Raw bytes ({len(raw_bytes)}): {raw_bytes.hex()}")

        # Check for OTA uplink (cmd type 0x20) first
        if len(raw_bytes) >= 2 and raw_bytes[0] == OTA_CMD_TYPE:
            decoded = decode_ota_uplink(raw_bytes)
            if decoded:
                print(f"Decoded as OTA uplink: {decoded.get('ota_type')}")
                return decoded

        # Check for diagnostics response (magic 0xE6)
        if len(raw_bytes) >= DIAG_PAYLOAD_SIZE and raw_bytes[0] == DIAG_MAGIC:
            decoded = decode_diag_payload(raw_bytes)
            if decoded:
                print("Decoded as diagnostics response")
                return decoded

        # Try new raw format first (magic byte 0xE5)
        decoded = decode_raw_evse_payload(raw_bytes)
        if decoded:
            print("Decoded as raw EVSE format")
            return decoded

        # Fall back to legacy sid_demo format
        decoded = decode_legacy_sid_demo_payload(raw_bytes)
        if decoded:
            print("Decoded as legacy sid_demo format")
            return decoded

        # If we can't parse, return raw for debugging
        return {
            'payload_type': 'unknown',
            'raw_hex': raw_bytes.hex(),
            'raw_length': len(raw_bytes)
        }

    except Exception as e:
        print(f"Error decoding payload: {e}")
        return {
            'payload_type': 'error',
            'error': str(e),
            'raw_payload': raw_payload_b64
        }


def store_transition_event(device_id, timestamp_ms, decoded):
    """Store an interlock transition event in DynamoDB (TASK-069).

    Written when the device reports a non-zero transition_reason, indicating
    a charge_allowed state change with a known cause.
    """
    reason = decoded.get('transition_reason', 'unknown')
    reason_code = decoded.get('transition_reason_code', 0)
    charge_allowed = decoded.get('charge_allowed', False)

    item = {
        'device_id': device_id,
        'timestamp': timestamp_ms + 1,  # +1ms to avoid PK collision with telemetry
        'ttl': int(timestamp_ms / 1000) + 7776000,  # 90-day retention
        'event_type': 'interlock_transition',
        'charge_allowed': charge_allowed,
        'transition_reason': reason,
        'transition_reason_code': reason_code,
    }

    # Include device-side timestamp if available
    if decoded.get('device_timestamp_unix'):
        item['device_timestamp_unix'] = decoded['device_timestamp_unix']
    if decoded.get('device_timestamp_epoch'):
        item['device_timestamp_epoch'] = decoded['device_timestamp_epoch']

    item = json.loads(json.dumps(item), parse_float=Decimal)
    table.put_item(Item=item)
    print(f"Stored transition: charge_allowed={charge_allowed}, reason={reason}")


def lambda_handler(event, context):
    """
    Process Sidewalk EVSE messages and store decoded data in DynamoDB.
    """
    print(f"Received event: {json.dumps(event)}")

    try:
        # Extract fields from Sidewalk message
        wireless_device_id = event.get('WirelessDeviceId', 'unknown')
        payload_data = event.get('PayloadData', '')

        sidewalk_metadata = event.get('WirelessMetadata', {}).get('Sidewalk', {})
        link_type = sidewalk_metadata.get('LinkType', 'unknown')
        rssi = sidewalk_metadata.get('Rssi', 0)
        seq = sidewalk_metadata.get('Seq', 0)
        timestamp_str = sidewalk_metadata.get('Timestamp', '')
        sidewalk_id = sidewalk_metadata.get('SidewalkId', '')

        # Decode the sensor payload
        decoded = decode_payload(payload_data)

        # Create DynamoDB item
        timestamp_ms = int(time.time() * 1000)
        ttl_seconds = int(timestamp_ms / 1000) + 7776000  # 90-day retention

        item = {
            'device_id': wireless_device_id,
            'timestamp': timestamp_ms,
            'ttl': ttl_seconds,
            'event_type': 'evse_telemetry',
            'device_type': 'evse',
            'schema_version': '2.1',
            'link_type': link_type,
            'rssi': rssi,
            'seq': seq,
            'sidewalk_id': sidewalk_id,
            'timestamp_str': timestamp_str,
            'raw_payload': payload_data,
        }

        # Add decoded sensor data
        if decoded.get('payload_type') == 'ota':
            item['event_type'] = 'ota_uplink'
            item['data'] = {'ota': decoded}

            # Forward OTA responses to the ota_sender Lambda (async)
            ota_type = decoded.get('ota_type', '')
            if ota_type in ('ack', 'complete', 'status'):
                ota_event = {'type': ota_type, **{k: v for k, v in decoded.items()
                             if k not in ('payload_type', 'ota_type')}}
                try:
                    lambda_client.invoke(
                        FunctionName=ota_lambda_name,
                        InvocationType='Event',  # async
                        Payload=json.dumps({'ota_event': ota_event}),
                    )
                    print(f"Forwarded OTA {ota_type} to {ota_lambda_name}")
                except Exception as e:
                    print(f"Failed to invoke OTA Lambda: {e}")

        elif decoded.get('payload_type') == 'diagnostics':
            item['event_type'] = 'device_diagnostics'
            item['data'] = {'diagnostics': decoded}

        elif decoded.get('payload_type') == 'evse':
            evse_data = {
                'format': decoded.get('format', 'unknown'),
                'version': decoded.get('version', 1),
                'pilot_state': decoded['j1772_state'],
                'pilot_state_code': decoded['j1772_state_code'],
                'pilot_voltage_mv': decoded['pilot_voltage_mv'],
                'current_draw_ma': decoded['current_ma'],
                'thermostat_bits': decoded['thermostat_bits'],
                'thermostat_cool_active': decoded['thermostat_cool'],
                'charge_allowed': decoded.get('charge_allowed', False),
                'charge_now': decoded.get('charge_now', False),
            }
            # v0x07 and earlier: include heat flag (removed in v0x08)
            if 'thermostat_heat' in decoded:
                evse_data['thermostat_heat_active'] = decoded['thermostat_heat']
            # Include fault flags if any are set
            if any(decoded.get(f) for f in ('fault_sensor', 'fault_clamp_mismatch',
                                             'fault_interlock', 'fault_selftest_fail')):
                evse_data['fault_sensor'] = decoded.get('fault_sensor', False)
                evse_data['fault_clamp_mismatch'] = decoded.get('fault_clamp_mismatch', False)
                evse_data['fault_interlock'] = decoded.get('fault_interlock', False)
                evse_data['fault_selftest_fail'] = decoded.get('fault_selftest_fail', False)
            # Include device-side timestamp if present (v0x07+)
            if decoded.get('device_timestamp_epoch') is not None:
                evse_data['device_timestamp_epoch'] = decoded['device_timestamp_epoch']
            if decoded.get('device_timestamp_unix') is not None:
                evse_data['device_timestamp_unix'] = decoded['device_timestamp_unix']
            item['data'] = {'evse': evse_data}

            # Auto-send TIME_SYNC on EVSE uplinks
            try:
                device_ts = decoded.get('device_timestamp_epoch')
                maybe_send_time_sync(wireless_device_id, device_timestamp=device_ts)
            except Exception as e:
                print(f"TIME_SYNC error: {e}")

            # Check scheduler divergence (TASK-071)
            try:
                check_scheduler_divergence(
                    wireless_device_id, decoded.get('charge_allowed', False))
            except Exception as e:
                print(f"Divergence check error: {e}")

            # Charge Now override (TASK-064 / ADR-003)
            if decoded.get('charge_now'):
                try:
                    handle_charge_now_override(wireless_device_id)
                except Exception as e:
                    print(f"Charge Now override error: {e}")

            # Interlock transition logging (TASK-069)
            reason_code = decoded.get('transition_reason_code', 0)
            if reason_code != 0:
                try:
                    store_transition_event(
                        wireless_device_id, timestamp_ms, decoded)
                except Exception as e:
                    print(f"Transition event error: {e}")
        else:
            item['data'] = {'decode_result': decoded}

        # Convert floats to Decimal for DynamoDB
        item = json.loads(json.dumps(item), parse_float=Decimal)

        # Write to DynamoDB
        table.put_item(Item=item)

        # Update device registry (best-effort, never block event processing)
        try:
            device_registry.get_or_create_device(registry_table, wireless_device_id, sidewalk_id)
            # Extract app_version from diagnostics/OTA, or app_build_version from v0x0A+ telemetry
            app_ver = decoded.get('app_version') if decoded.get('payload_type') in ('ota', 'diagnostics') else None
            if app_ver is None and decoded.get('payload_type') == 'evse':
                app_ver = decoded.get('app_build_version')
            device_registry.update_last_seen(registry_table, wireless_device_id, app_version=app_ver)
        except Exception as e:
            print(f"Device registry update failed (non-fatal): {e}")

        print(f"Stored decoded EVSE data: {json.dumps(decoded)}")

        return {
            'statusCode': 200,
            'body': json.dumps({
                'message': 'EVSE data processed',
                'device_id': wireless_device_id,
                'decoded': decoded
            })
        }

    except Exception as e:
        print(f"Error processing message: {e}")
        raise
