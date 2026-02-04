"""
Lambda function to decode EVSE Sidewalk sensor data.

Decodes the sid_demo_parser format and extracts:
- J1772 pilot state
- Pilot voltage (mV)
- Current draw (mA)
- Thermostat input bits
"""

import json
import base64
import boto3
import time
from decimal import Decimal

dynamodb = boto3.resource('dynamodb')
table = dynamodb.Table('sidewalk-v1-device_events_v2')

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


def decode_sid_demo_payload(raw_payload_b64):
    """
    Decode the sid_demo format payload.

    The payload structure from our device:
    - Wrapped in sid_demo format (header bytes)
    - Inner payload is evse_payload_t:
      - payload_type: 1 byte (0x01 = EVSE)
      - j1772_state: 1 byte
      - pilot_voltage: 2 bytes (little-endian, mV)
      - current_ma: 2 bytes (little-endian, mA)
      - thermostat_bits: 1 byte
    """
    try:
        # Decode base64 - the payload is ASCII hex encoded after base64
        ascii_hex = base64.b64decode(raw_payload_b64).decode('ascii')

        # Convert ASCII hex to bytes
        raw_bytes = bytes.fromhex(ascii_hex)

        print(f"Raw bytes ({len(raw_bytes)}): {raw_bytes.hex()}")

        # Parse sid_demo format
        # Format: [demo_header...][payload]
        # The demo header varies, but our payload starts after it

        # Find the EVSE payload (type 0x01)
        # Based on the observed data: 4081006d012082000102030b010c04
        # Breaking it down:
        # 40 81 00 6d 01 20 82 00 01 02 03 0b 01 0c 04
        #                            ^^ payload_type (could be here)

        # The sid_demo format wraps our data. Let's find the sensor data.
        # Looking at the structure, the last bytes contain our actual values

        # Try to find a pattern - look for sequences that make sense as sensor data
        # Expected: type(1) + j1772(1) + voltage(2) + current(2) + therm(1) = 7 bytes

        if len(raw_bytes) >= 7:
            # Try parsing from different offsets to find valid data
            # The demo format has variable header length

            # Based on observed data pattern, try offset after demo headers
            for offset in range(len(raw_bytes) - 6):
                payload_type = raw_bytes[offset]
                if payload_type == 0x01:  # EVSE type
                    j1772_state = raw_bytes[offset + 1]
                    if j1772_state <= 6:  # Valid J1772 state
                        pilot_voltage = int.from_bytes(raw_bytes[offset + 2:offset + 4], 'little')
                        current_ma = int.from_bytes(raw_bytes[offset + 4:offset + 6], 'little')
                        thermostat_bits = raw_bytes[offset + 6] if offset + 6 < len(raw_bytes) else 0

                        # Sanity check values
                        if 0 <= pilot_voltage <= 15000 and 0 <= current_ma <= 100000:
                            return {
                                'payload_type': 'evse',
                                'j1772_state_code': j1772_state,
                                'j1772_state': J1772_STATES.get(j1772_state, 'UNKNOWN'),
                                'pilot_voltage_mv': pilot_voltage,
                                'current_ma': current_ma,
                                'thermostat_bits': thermostat_bits,
                                'thermostat_1': bool(thermostat_bits & 0x01),
                                'thermostat_2': bool(thermostat_bits & 0x02),
                            }

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
        decoded = decode_sid_demo_payload(payload_data)

        # Create DynamoDB item
        timestamp_ms = int(time.time() * 1000)

        item = {
            'device_id': wireless_device_id,
            'timestamp': timestamp_ms,
            'event_type': 'evse_telemetry',
            'device_type': 'evse',
            'schema_version': '2.0',
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
                    'pilot_state': decoded['j1772_state'],
                    'pilot_state_code': decoded['j1772_state_code'],
                    'pilot_voltage_mv': decoded['pilot_voltage_mv'],
                    'current_draw_ma': decoded['current_ma'],
                    'thermostat_bits': decoded['thermostat_bits'],
                    'thermostat_1_active': decoded['thermostat_1'],
                    'thermostat_2_active': decoded['thermostat_2'],
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
