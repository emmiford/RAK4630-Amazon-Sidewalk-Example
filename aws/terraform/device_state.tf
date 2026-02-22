# --- EVSE Device State (ADR-006) ---
# Per-device mutable state snapshot, overwritten on each uplink.
# Replaces sentinel keys (timestamp 0, -1, -2, -3) in the events table.
# Enables O(1) fleet overview via table scan.

resource "aws_dynamodb_table" "device_state" {
  name         = var.device_state_table_name
  billing_mode = "PAY_PER_REQUEST"
  hash_key     = "device_id"

  attribute {
    name = "device_id"
    type = "S"
  }

  tags = {
    Project     = "evse-monitor"
    Environment = var.environment
  }
}
