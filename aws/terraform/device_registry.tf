# --- SideCharge Device Registry ---
# DynamoDB table for device management with human-readable SC-XXXXXXXX IDs.
# Created by TASK-036; consumed by decode Lambda on each uplink.

resource "aws_dynamodb_table" "device_registry" {
  name         = var.device_registry_table_name
  billing_mode = "PAY_PER_REQUEST"
  hash_key     = "device_id"

  attribute {
    name = "device_id"
    type = "S"
  }

  attribute {
    name = "owner_email"
    type = "S"
  }

  attribute {
    name = "status"
    type = "S"
  }

  global_secondary_index {
    name            = "owner_email-index"
    hash_key        = "owner_email"
    projection_type = "ALL"
  }

  global_secondary_index {
    name            = "status-index"
    hash_key        = "status"
    projection_type = "ALL"
  }

  tags = {
    Project     = "evse-monitor"
    Environment = var.environment
  }
}
