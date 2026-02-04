"""
Lambda function to decode EVSE Sidewalk sensor data.

Supports two payload formats:
1. New raw format (8 bytes): Magic 0xE5, Version, J1772 state, voltage, current, thermostat
2. Legacy sid_demo format: Wrapped with demo protocol headers

Extracts:
- J1772 pilot state
- Pilot voltage (mV)
- Current draw (mA)
- Thermostat input bits
"""

import json
import base64
import os
import boto3
import time
from decimal import Decimal

dynamodb = boto3.resource('dynamodb')
table_name = os.environ.get('DYNAMODB_TABLE', 'sidewalk-v1-device_events_v2')
table = dynamodb.Table(table_name)

# EVSE payload magic byte and version
EVSE_MAGIC = 0xE5
EVSE_VERSION = 0x01
EVSE_PAYLOAD_SIZE = 8

# Legacy payload type
LEGACY_EVSE_TYPE = 0x01

# J1772 state mapping
J1772_STATES = {
    0: 'UNKNOWN',
    1: 'A',  # No vehicle (12V)
    2: 'B',  # Vehicle connected, not ready (9V)
    3: 'C',  # Vehicle ready, charging (6V)
    4: 'D',  # Vehicle ready, ventilation required (3V)
    5: 'E',  # Error - short circuit
    6: 'F',  # Error - no pilot
}


def decode_raw_evse_payload(raw_bytes):
    """
    Decode the new raw 8-byte EVSE payload format.

    Format:
      Byte 0: Magic (0xE5)
      Byte 1: Version (0x01)
      Byte 2: J1772 state (0-6)
      Byte 3-4: Pilot voltage mV (little-endian)
      Byte 5-6: Current mA (little-endian)
      Byte 7: Thermostat flags
    """
    if len(raw_bytes) < EVSE_PAYLOAD_SIZE:
        return None

    magic = raw_bytes[0]
    version = raw_bytes[1]

    if magic != EVSE_MAGIC:
        return None

    j1772_state = raw_bytes[2]
    pilot_voltage = int.from_bytes(raw_bytes[3:5], 'little')
    current_ma = int.from_bytes(raw_bytes[5:7], 'little')
    thermostat_bits = raw_bytes[7]

    # Sanity check values
    if j1772_state > 6:
        return None
    if pilot_voltage > 15000 or current_ma > 100000:
        return None

    return {
        'payload_type': 'evse',
        'format': 'raw_v1',
        'version': version,
        'j1772_state_code': j1772_state,
        'j1772_state': J1772_STATES.get(j1772_state, 'UNKNOWN'),
        'pilot_voltage_mv': pilot_voltage,
        'current_ma': current_ma,
        'thermostat_bits': thermostat_bits,
        'thermostat_heat': bool(thermostat_bits & 0x01),
        'thermostat_cool': bool(thermostat_bits & 0x02),
    }


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

        # Try new raw format first (magic byte 0xE5)
        decoded = decode_raw_evse_payload(raw_bytes)
        if decoded:
            print(f"Decoded as raw EVSE format")
            return decoded

        # Fall back to legacy sid_demo format
        decoded = decode_legacy_sid_demo_payload(raw_bytes)
        if decoded:
            print(f"Decoded as legacy sid_demo format")
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

        item = {
            'device_id': wireless_device_id,
            'timestamp': timestamp_ms,
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
        if decoded.get('payload_type') == 'evse':
            item['data'] = {
                'evse': {
                    'format': decoded.get('format', 'unknown'),
                    'pilot_state': decoded['j1772_state'],
                    'pilot_state_code': decoded['j1772_state_code'],
                    'pilot_voltage_mv': decoded['pilot_voltage_mv'],
                    'current_draw_ma': decoded['current_ma'],
                    'thermostat_bits': decoded['thermostat_bits'],
                    'thermostat_heat_active': decoded['thermostat_heat'],
                    'thermostat_cool_active': decoded['thermostat_cool'],
                }
            }
        else:
            item['data'] = {'decode_result': decoded}

        # Convert floats to Decimal for DynamoDB
        item = json.loads(json.dumps(item), parse_float=Decimal)

        # Write to DynamoDB
        table.put_item(Item=item)

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
