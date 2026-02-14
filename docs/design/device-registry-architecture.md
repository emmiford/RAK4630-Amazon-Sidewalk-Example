# TASK-036: SideCharge Device Registry -- Technical Architecture

**Author**: Eliel (Backend Architect)
**Date**: 2026-02-13
**Status**: DESIGN
**PRD Reference**: Section 4.6, PDL-012
**Traceability**: TASK-036 -> PRD 4.6 (Device Registry)

---

## 1. DynamoDB Table Design

### 1.1 Primary Table: `sidecharge-device-registry`

**Billing**: PAY_PER_REQUEST (on-demand). At 1--10 devices this costs fractions of a cent. Scales to 1000+ without provisioned capacity planning.

| Attribute | DynamoDB Type | Description |
|-----------|---------------|-------------|
| `device_id` **(PK)** | `S` | `SC-XXXXXXXX` -- derived short ID |
| `sidewalk_id` | `S` | Full Sidewalk `WirelessDeviceId` UUID (36 chars) |
| `owner_name` | `S` | Customer name |
| `owner_email` | `S` | Customer contact email |
| `meter_number` | `S` | Utility electric meter number (future: TASK-037) |
| `install_address` | `S` | Street address of installation |
| `install_lat` | `N` | Latitude (optional) |
| `install_lon` | `N` | Longitude (optional) |
| `install_date` | `S` | ISO 8601 -- when electrician installed |
| `installer_name` | `S` | Who performed the installation |
| `provisioned_date` | `S` | ISO 8601 -- when MFG credentials were flashed |
| `app_version` | `N` | Last known app firmware version (from uplink byte 1) |
| `platform_version` | `N` | Last known platform version (if reported) |
| `last_seen` | `S` | ISO 8601 -- timestamp of most recent uplink |
| `last_seen_epoch` | `N` | Unix epoch seconds of most recent uplink (for GSI range queries) |
| `status` | `S` | `provisioned` / `installed` / `active` / `inactive` / `returned` |
| `created_at` | `S` | ISO 8601 -- record creation time |
| `updated_at` | `S` | ISO 8601 -- last modification time |
| `notes` | `S` | Free-text field for installer/support notes |

**Design rationale -- partition key choice**:

The partition key is `device_id` (the short ID), not `sidewalk_id`. Reasons:

1. Every human interaction (support calls, labels, search boxes, CLI tools) uses the short ID. The PK should match the most common access pattern.
2. The Sidewalk UUID is a lookup path, not the primary identity. A GSI on `sidewalk_id` handles that.
3. The short ID is deterministically derived from the Sidewalk UUID, so there is a provable 1:1 mapping -- no ambiguity.

**No sort key**. This is a registry, not a time series. Each device has exactly one record. The events table (`sidewalk-v1-device_events_v2`) already handles time-series telemetry with `device_id` + `timestamp`.

### 1.2 Global Secondary Indexes (GSIs)

| GSI Name | Partition Key | Sort Key | Projection | Purpose |
|----------|---------------|----------|------------|---------|
| `sidewalk-id-index` | `sidewalk_id` (S) | -- | `KEYS_ONLY` | Decode Lambda looks up device by Sidewalk UUID on every uplink |
| `status-last-seen-index` | `status` (S) | `last_seen_epoch` (N) | `ALL` | Fleet health: list all `active` devices sorted by last seen. Offline detection scans this. |
| `installer-index` | `installer_name` (S) | `install_date` (S) | `ALL` | List devices by installer (for installer dashboard / audit) |

**GSIs deferred to v2** (not needed at 1--10 devices):

- `owner-email-index` on `owner_email` -- "my devices" lookup. At small scale, a Scan with FilterExpression is fine.
- `firmware-version-index` on `app_version` -- fleet firmware audit. At small scale, Scan.

**Why `KEYS_ONLY` for `sidewalk-id-index`**: The decode Lambda needs only the `device_id` to update `last_seen`. It does a GSI Query to resolve `sidewalk_id` -> `device_id`, then does an UpdateItem on the main table. Projecting all attributes would double storage cost for no benefit.

### 1.3 Query Pattern Coverage

| Query | Method | Cost |
|-------|--------|------|
| Lookup by short ID (`SC-a3f7c021`) | GetItem on PK | 0.5 RCU |
| Lookup by Sidewalk UUID | Query `sidewalk-id-index` | 0.5 RCU |
| List all active devices sorted by last seen | Query `status-last-seen-index` with `status=active` | Proportional to result count |
| List devices by installer | Query `installer-index` | Proportional to result count |
| Filter by firmware version | Scan with FilterExpression on `app_version` | Full table scan (acceptable at <1000 devices) |
| Detect offline devices | Query `status-last-seen-index` with `status=active` and `last_seen_epoch < threshold` | Single query |
| List all devices | Scan | Full table scan (acceptable at <1000 devices) |

---

## 2. Short ID Derivation

### 2.1 Algorithm

```
short_id = "SC-" + lowercase_hex(SHA-256(sidewalk_uuid))[:8]
```

- Input: The Sidewalk `WirelessDeviceId` UUID string exactly as returned by AWS IoT Wireless (e.g., `b319d001-6b08-4d88-b4ca-4d2d98a6d43c`)
- The UUID string is hashed as-is (lowercase with hyphens) -- no normalization. AWS IoT Wireless always returns lowercase UUIDs.
- Output example: `SC-a3f7c021`

### 2.2 Python Implementation

```python
import hashlib


def derive_device_id(sidewalk_uuid: str) -> str:
    """
    Derive SideCharge short device ID from Sidewalk UUID.

    Format: SC- + first 8 hex chars of SHA-256(sidewalk_uuid)
    Example: derive_device_id("b319d001-6b08-4d88-b4ca-4d2d98a6d43c")
             -> "SC-e84c9a7f"

    The Sidewalk UUID is hashed as-is (lowercase, with hyphens).
    """
    digest = hashlib.sha256(sidewalk_uuid.encode("utf-8")).hexdigest()
    return f"SC-{digest[:8]}"
```

### 2.3 Collision Analysis

8 hex characters = 32 bits = ~4.29 billion unique values. Birthday paradox gives 50% collision probability at ~65,000 devices. For a fleet of 1,000 devices the collision probability is approximately 0.01%. For 10,000 devices it rises to ~1%. If the fleet ever approaches 10K+, extend to 10 or 12 hex characters (trivial change -- the SHA-256 digest has 64 hex chars available).

For v1.0 with 1--10 devices, collision risk is effectively zero.

### 2.4 Where This Runs

The `derive_device_id()` function lives in `sidewalk_utils.py` (shared Lambda utility module). It is called:

1. In the decode Lambda on first uplink (auto-registration)
2. In the registry API Lambda when creating/looking up devices
3. In the `ota_deploy.py` CLI for display purposes

The function is pure (no side effects, no network calls), deterministic, and testable in isolation.

---

## 3. Registration Flow

### 3.1 Lifecycle States

```
  Factory          Installer           First Uplink       No Uplinks        Device Returned
    |                  |                    |              for 45 min              |
    v                  v                    v                  v                   v
PROVISIONED -----> INSTALLED ---------> ACTIVE ---------> INACTIVE ---------> RETURNED
    |                                       ^                  |
    |                                       +------------------+
    |                                       (uplink resumes)
    +--- (first uplink, no installer step) --> ACTIVE
         (auto-registration shortcut for dev/prototyping)
```

### 3.2 v1.0 Minimum Viable Registration Flow

For v1.0 with 1--10 devices, the flow has two entry points and both are practical:

**Path A: Auto-registration on first uplink (no installer step)**

This is the critical path for v1.0 -- it means the system works even before the installer API exists.

1. Device powers on, connects to Sidewalk, sends first uplink.
2. Decode Lambda receives the uplink with `WirelessDeviceId` (Sidewalk UUID).
3. Decode Lambda calls `derive_device_id(sidewalk_uuid)` to compute short ID.
4. Decode Lambda does a `GetItem` on the registry table with the computed `device_id`.
5. If no record exists: `PutItem` creates a new record with:
   - `device_id`: computed short ID
   - `sidewalk_id`: the UUID from the uplink
   - `status`: `active` (skip `provisioned`/`installed` for auto-registration)
   - `app_version`: extracted from uplink payload (byte 1)
   - `last_seen`: current ISO 8601 timestamp
   - `last_seen_epoch`: current Unix epoch seconds
   - `created_at`: current ISO 8601 timestamp
   - `updated_at`: current ISO 8601 timestamp
   - All other fields: omitted (DynamoDB is schemaless -- they can be added later)
6. If record exists: `UpdateItem` to set `last_seen`, `last_seen_epoch`, `app_version`, `updated_at`.

**Path B: Factory provisioning + installer commissioning**

For production devices:

1. **Factory**: When MFG credentials are flashed, a provisioning script creates the registry record:
   - `device_id`: computed from the Sidewalk UUID assigned during provisioning
   - `sidewalk_id`: the UUID
   - `status`: `provisioned`
   - `provisioned_date`: now
   - Device label is printed with the `SC-XXXXXXXX` short ID
2. **Installation**: Installer uses a CLI tool (or future app) to update the record:
   - `owner_name`, `owner_email`, `install_address`, `install_lat`/`install_lon`
   - `installer_name`, `install_date`
   - `meter_number` (TASK-037)
   - `status`: `installed`
3. **First uplink**: Decode Lambda sees the existing record, transitions `status` to `active`, starts updating `last_seen` and `app_version`.

### 3.3 Why Auto-Registration Is the Right v1.0 Default

- Zero setup friction -- the device just works.
- The decode Lambda already receives every uplink. Adding a registry write is ~5 lines of code.
- Installer metadata (owner, address) can be backfilled at any time via CLI or API.
- For 1--10 prototype/dev devices, the operator (Emily) knows who owns each device. Formal commissioning flow is a v2 concern.

---

## 4. Data Model Details

### 4.1 Status Field Semantics

| Status | Meaning | Entered When | Exited When |
|--------|---------|--------------|-------------|
| `provisioned` | MFG flashed, not yet installed | Factory provisioning script | Installer updates record |
| `installed` | Electrician installed, awaiting first uplink | Installer CLI/API call | First uplink received |
| `active` | Device is communicating normally | First uplink (auto or post-install) | No uplink for 45 minutes |
| `inactive` | Device has gone silent | Liveness checker detects stale `last_seen` | Uplink resumes |
| `returned` | Device decommissioned / returned by customer | Manual operator action | -- (terminal state, can be re-provisioned) |

### 4.2 Attribute Conventions

- **Timestamps**: All ISO 8601 strings in UTC (e.g., `2026-02-13T18:45:00Z`). The `last_seen_epoch` numeric field exists solely for GSI range queries (DynamoDB cannot do range comparisons on ISO 8601 strings efficiently).
- **Optional fields**: DynamoDB does not enforce a schema. Fields like `install_lat`, `install_lon`, `meter_number`, `notes` are simply absent until set. No null values -- omit the attribute entirely.
- **`app_version`**: Integer matching the `EVSE_VERSION` byte in the uplink payload (byte 1). Currently `0x05`. This is the app firmware version, not the platform version.
- **`device_id` format**: Always lowercase. `SC-a3f7c021`, never `SC-A3F7C021`. The `derive_device_id()` function enforces this (SHA-256 hexdigest is lowercase by default in Python).

### 4.3 Relationship to Existing Events Table

The device registry (`sidecharge-device-registry`) and the events table (`sidewalk-v1-device_events_v2`) are separate tables with different purposes:

| | Registry Table | Events Table |
|-|----------------|--------------|
| **Purpose** | Device identity, ownership, config | Time-series telemetry |
| **PK** | `device_id` (short ID) | `device_id` (Sidewalk UUID) |
| **Records per device** | 1 | Thousands (one per uplink) |
| **TTL** | None (permanent) | 14 days (existing TTL) |
| **Updated by** | Decode Lambda, installer API | Decode Lambda |

**Important**: The events table currently uses the full Sidewalk UUID as its `device_id`. The registry table uses the short ID. The decode Lambda bridges them: it receives the Sidewalk UUID in each uplink, resolves it to a short ID via the registry, and writes to both tables. The events table's PK stays as-is (no migration needed).

---

## 5. Liveness Tracking

### 5.1 Chosen Approach: CloudWatch Scheduled Lambda

**Option (b)** from the design brief: a lightweight Lambda on a 15-minute EventBridge schedule that scans the `status-last-seen-index` GSI.

**Why this option**:
- The device heartbeat is 15 minutes (PDL-010). A device is "offline" if no uplink for 3 heartbeat periods (45 minutes) -- this accounts for one missed heartbeat plus LoRa delivery variance.
- DynamoDB Streams + TTL (option a) is over-engineered: TTL deletion is best-effort and can lag by hours. We need timely detection, not eventual consistency.
- EventBridge "no-event" detection (option c) requires one rule per device. Does not scale and is complex to manage.

### 5.2 Liveness Checker Lambda

```
Name:     device-liveness-checker
Trigger:  EventBridge rate(15 minutes)
Runtime:  python3.11
Timeout:  30s
Memory:   128MB
```

**Algorithm**:

1. Query `status-last-seen-index` with `status = "active"` and `last_seen_epoch < (now - 2700)` (45 minutes ago).
2. For each stale device: `UpdateItem` to set `status = "inactive"`, `updated_at = now`.
3. Log the transition. (Future: SNS notification to operator.)

**Reverse transition**: The decode Lambda, on every uplink, checks if `status == "inactive"`. If so, it transitions back to `active`. This means a device that comes back online is automatically re-activated with no manual intervention.

### 5.3 Why 45 Minutes (Not 30)

The heartbeat is every 15 minutes. LoRa is lossy -- a single missed uplink is normal operation, not a failure. Two consecutive missed uplinks (30 minutes of silence) could be a temporary RF issue. Three missed uplinks (45 minutes) with high confidence indicates the device is actually offline (power loss, hardware failure, or gateway outage).

At 45 minutes, the false positive rate is near zero for a device that's actually running. The cost of a false positive (unnecessary `inactive` status, operator attention) is higher than the cost of 15 extra minutes of detection latency.

### 5.4 Future Enhancement: CloudWatch Alarm

For production with SNS notifications, a CloudWatch custom metric could track `active_device_count`. A drop triggers an alarm. This is a v2 feature -- for v1.0, the liveness Lambda + CloudWatch logs are sufficient for an operator who checks the dashboard.

---

## 6. Terraform Changes

### 6.1 New Resources

The following resources are added to `aws/terraform/main.tf`:

**DynamoDB Table**:
```hcl
resource "aws_dynamodb_table" "device_registry" {
  name         = var.device_registry_table_name
  billing_mode = "PAY_PER_REQUEST"
  hash_key     = "device_id"

  attribute {
    name = "device_id"
    type = "S"
  }

  attribute {
    name = "sidewalk_id"
    type = "S"
  }

  attribute {
    name = "status"
    type = "S"
  }

  attribute {
    name = "last_seen_epoch"
    type = "N"
  }

  attribute {
    name = "installer_name"
    type = "S"
  }

  attribute {
    name = "install_date"
    type = "S"
  }

  global_secondary_index {
    name            = "sidewalk-id-index"
    hash_key        = "sidewalk_id"
    projection_type = "KEYS_ONLY"
  }

  global_secondary_index {
    name            = "status-last-seen-index"
    hash_key        = "status"
    range_key       = "last_seen_epoch"
    projection_type = "ALL"
  }

  global_secondary_index {
    name            = "installer-index"
    hash_key        = "installer_name"
    range_key       = "install_date"
    projection_type = "ALL"
  }

  tags = {
    Project     = "evse-monitor"
    Environment = var.environment
  }
}
```

**Liveness Checker Lambda** (IAM role, policy, function, EventBridge rule, target, permission):
```hcl
# Package liveness checker Lambda
data "archive_file" "liveness_checker_zip" {
  type        = "zip"
  output_path = "${path.module}/../device_liveness_lambda.zip"

  source {
    content  = file("${path.module}/../device_liveness_lambda.py")
    filename = "device_liveness_lambda.py"
  }
}

resource "aws_iam_role" "liveness_checker_role" {
  name = "device-liveness-checker-role"

  assume_role_policy = jsonencode({
    Version = "2012-10-17"
    Statement = [{
      Action    = "sts:AssumeRole"
      Effect    = "Allow"
      Principal = { Service = "lambda.amazonaws.com" }
    }]
  })
}

resource "aws_iam_role_policy" "liveness_checker_policy" {
  name = "device-liveness-checker-policy"
  role = aws_iam_role.liveness_checker_role.id

  policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Effect   = "Allow"
        Action   = ["logs:CreateLogGroup", "logs:CreateLogStream", "logs:PutLogEvents"]
        Resource = "arn:aws:logs:*:*:*"
      },
      {
        Effect = "Allow"
        Action = [
          "dynamodb:Query",
          "dynamodb:UpdateItem"
        ]
        Resource = [
          aws_dynamodb_table.device_registry.arn,
          "${aws_dynamodb_table.device_registry.arn}/index/*"
        ]
      }
    ]
  })
}

resource "aws_lambda_function" "liveness_checker" {
  filename         = data.archive_file.liveness_checker_zip.output_path
  function_name    = "device-liveness-checker"
  role             = aws_iam_role.liveness_checker_role.arn
  handler          = "device_liveness_lambda.lambda_handler"
  source_code_hash = data.archive_file.liveness_checker_zip.output_base64sha256
  runtime          = "python3.11"
  timeout          = 30
  memory_size      = 128

  environment {
    variables = {
      REGISTRY_TABLE    = var.device_registry_table_name
      OFFLINE_THRESHOLD = "2700"  # 45 minutes in seconds
    }
  }

  tags = {
    Project     = "evse-monitor"
    Environment = var.environment
  }
}

resource "aws_cloudwatch_log_group" "liveness_checker_logs" {
  name              = "/aws/lambda/device-liveness-checker"
  retention_in_days = 14
}

resource "aws_cloudwatch_event_rule" "liveness_check" {
  name                = "device-liveness-check"
  schedule_expression = "rate(15 minutes)"

  tags = {
    Project     = "evse-monitor"
    Environment = var.environment
  }
}

resource "aws_cloudwatch_event_target" "liveness_check_target" {
  rule      = aws_cloudwatch_event_rule.liveness_check.name
  target_id = "device-liveness-checker"
  arn       = aws_lambda_function.liveness_checker.arn
}

resource "aws_lambda_permission" "eventbridge_invoke_liveness" {
  statement_id  = "AllowEventBridgeInvokeLiveness"
  action        = "lambda:InvokeFunction"
  function_name = aws_lambda_function.liveness_checker.function_name
  principal     = "events.amazonaws.com"
  source_arn    = aws_cloudwatch_event_rule.liveness_check.arn
}
```

### 6.2 New Variables

Add to `aws/terraform/variables.tf`:

```hcl
variable "device_registry_table_name" {
  description = "Name of the DynamoDB table for device registry"
  type        = string
  default     = "sidecharge-device-registry"
}
```

### 6.3 IAM Policy Updates for Existing Lambdas

**Decode Lambda** (`evse_decoder_policy`) -- add registry read/write:

```hcl
{
  Effect = "Allow"
  Action = [
    "dynamodb:GetItem",
    "dynamodb:PutItem",
    "dynamodb:UpdateItem",
    "dynamodb:Query"
  ]
  Resource = [
    "arn:aws:dynamodb:${var.aws_region}:*:table/${var.device_registry_table_name}",
    "arn:aws:dynamodb:${var.aws_region}:*:table/${var.device_registry_table_name}/index/*"
  ]
}
```

**Charge Scheduler Lambda** (`charge_scheduler_policy`) -- add registry read:

```hcl
{
  Effect = "Allow"
  Action = [
    "dynamodb:GetItem",
    "dynamodb:Query",
    "dynamodb:Scan"
  ]
  Resource = [
    "arn:aws:dynamodb:${var.aws_region}:*:table/${var.device_registry_table_name}",
    "arn:aws:dynamodb:${var.aws_region}:*:table/${var.device_registry_table_name}/index/*"
  ]
}
```

**Decode Lambda environment variable** -- add `REGISTRY_TABLE`:

```hcl
environment {
  variables = {
    DYNAMODB_TABLE  = var.dynamodb_table_name
    REGISTRY_TABLE  = var.device_registry_table_name
    OTA_LAMBDA_NAME = aws_lambda_function.ota_sender.function_name
  }
}
```

### 6.4 New Outputs

Add to `aws/terraform/outputs.tf`:

```hcl
output "device_registry_table_name" {
  description = "Name of the device registry DynamoDB table"
  value       = aws_dynamodb_table.device_registry.name
}

output "device_registry_table_arn" {
  description = "ARN of the device registry DynamoDB table"
  value       = aws_dynamodb_table.device_registry.arn
}

output "liveness_checker_lambda_arn" {
  description = "ARN of the device liveness checker Lambda"
  value       = aws_lambda_function.liveness_checker.arn
}
```

---

## 7. Integration with Existing Lambdas

### 7.1 Decode Lambda Changes (`decode_evse_lambda.py`)

The decode Lambda is the integration point. It receives every uplink and is the natural place to update the registry. Changes are additive -- existing telemetry processing is untouched.

**New module-level setup** (add near top of file):

```python
registry_table_name = os.environ.get('REGISTRY_TABLE', 'sidecharge-device-registry')
registry_table = dynamodb.Table(registry_table_name)
```

**New import** in `decode_evse_lambda.py`:

```python
from sidewalk_utils import derive_device_id
```

**New function** -- `update_device_registry()`:

```python
def update_device_registry(sidewalk_uuid, app_version=None):
    """
    Update (or auto-create) device registry entry on each uplink.

    - If device exists: update last_seen, app_version, updated_at.
      If status is 'inactive', transition back to 'active'.
    - If device does not exist: auto-register with status 'active'.
    """
    device_id = derive_device_id(sidewalk_uuid)
    now_iso = datetime.utcnow().strftime('%Y-%m-%dT%H:%M:%SZ')
    now_epoch = int(time.time())

    try:
        # Try conditional update first (device exists)
        update_expr = "SET last_seen = :ls, last_seen_epoch = :lse, updated_at = :ua"
        expr_values = {
            ':ls': now_iso,
            ':lse': now_epoch,
            ':ua': now_iso,
        }
        if app_version is not None:
            update_expr += ", app_version = :av"
            expr_values[':av'] = app_version

        # Reactivate if inactive
        update_expr += ", #st = if_not_exists(#st, :active)"
        expr_names = {'#st': 'status'}
        # Actually, we want to set active only if currently inactive.
        # Use a simpler approach: always update, then fix status.

        registry_table.update_item(
            Key={'device_id': device_id},
            UpdateExpression=update_expr,
            ExpressionAttributeValues=expr_values,
            ExpressionAttributeNames=expr_names,
        )

        # Check and reactivate if inactive (separate call to keep update simple)
        resp = registry_table.get_item(
            Key={'device_id': device_id},
            ProjectionExpression='#st',
            ExpressionAttributeNames={'#st': 'status'},
        )
        item = resp.get('Item')
        if item and item.get('status') == 'inactive':
            registry_table.update_item(
                Key={'device_id': device_id},
                UpdateExpression='SET #st = :s, updated_at = :ua',
                ExpressionAttributeNames={'#st': 'status'},
                ExpressionAttributeValues={':s': 'active', ':ua': now_iso},
            )
            print(f"Registry: {device_id} reactivated (was inactive)")

        print(f"Registry: updated {device_id}")

    except registry_table.meta.client.exceptions.ConditionalCheckFailedException:
        pass  # Should not happen with this pattern
    except Exception as e:
        # Auto-register: create new record
        # (This handles the case where the item doesn't exist yet)
        try:
            registry_table.put_item(
                Item={
                    'device_id': device_id,
                    'sidewalk_id': sidewalk_uuid,
                    'status': 'active',
                    'app_version': app_version if app_version is not None else 0,
                    'last_seen': now_iso,
                    'last_seen_epoch': now_epoch,
                    'created_at': now_iso,
                    'updated_at': now_iso,
                },
                ConditionExpression='attribute_not_exists(device_id)',
            )
            print(f"Registry: auto-registered {device_id} ({sidewalk_uuid})")
        except Exception as e2:
            print(f"Registry: auto-register failed for {device_id}: {e2}")
```

**Simpler alternative** (recommended for v1.0 -- fewer DynamoDB calls):

```python
def update_device_registry(sidewalk_uuid, app_version=None):
    """
    Upsert device registry entry on each uplink.

    Uses UpdateItem with SET + if_not_exists for auto-registration.
    Single DynamoDB call handles both create and update.
    """
    device_id = derive_device_id(sidewalk_uuid)
    now_iso = datetime.utcnow().strftime('%Y-%m-%dT%H:%M:%SZ')
    now_epoch = int(time.time())

    update_expr = (
        "SET last_seen = :ls, last_seen_epoch = :lse, updated_at = :ua, "
        "sidewalk_id = if_not_exists(sidewalk_id, :sid), "
        "#st = if_not_exists(#st, :active), "
        "created_at = if_not_exists(created_at, :ca)"
    )
    expr_values = {
        ':ls': now_iso,
        ':lse': now_epoch,
        ':ua': now_iso,
        ':sid': sidewalk_uuid,
        ':active': 'active',
        ':ca': now_iso,
    }
    expr_names = {'#st': 'status'}

    if app_version is not None:
        update_expr += ", app_version = :av"
        expr_values[':av'] = app_version

    try:
        registry_table.update_item(
            Key={'device_id': device_id},
            UpdateExpression=update_expr,
            ExpressionAttributeValues=expr_values,
            ExpressionAttributeNames=expr_names,
        )
        print(f"Registry: upserted {device_id}")
    except Exception as e:
        print(f"Registry: update failed for {device_id}: {e}")
```

This single-call approach uses `if_not_exists()` for fields that should only be set on first registration (`sidewalk_id`, `status`, `created_at`). The `last_seen`, `last_seen_epoch`, `updated_at`, and `app_version` fields are always overwritten.

**Reactivation** is handled separately: after the upsert, if the device was `inactive`, a second conditional UpdateItem sets `status = "active"`. This costs one extra read on every uplink to check status. At v1.0 scale this is negligible. At 1000+ devices, the reactivation check could be moved to the liveness Lambda (it already knows which devices are inactive).

**Call site** in `lambda_handler()`, after decoding the payload:

```python
# Extract app_version from EVSE payload
app_version = None
if decoded.get('payload_type') == 'evse':
    app_version = decoded.get('version')

# Update device registry (auto-registers on first uplink)
update_device_registry(wireless_device_id, app_version)
```

### 7.2 Charge Scheduler Lambda Changes (`charge_scheduler_lambda.py`)

The scheduler currently hardcodes a single device via `sidewalk_utils.get_device_id()`. With the registry, it can iterate over all `active` devices:

```python
def get_active_devices():
    """Query registry for all active devices."""
    registry = dynamodb.Table(os.environ.get('REGISTRY_TABLE', 'sidecharge-device-registry'))
    resp = registry.query(
        IndexName='status-last-seen-index',
        KeyConditionExpression='#st = :active',
        ExpressionAttributeNames={'#st': 'status'},
        ExpressionAttributeValues={':active': 'active'},
    )
    return resp.get('Items', [])
```

This is a **v1.1 change** -- for v1.0, the scheduler continues to use `get_device_id()` for the single device. The registry integration for the scheduler becomes important when supporting per-device TOU schedules (TASK-037) and multiple devices.

### 7.3 `sidewalk_utils.py` Changes

Add `derive_device_id()` to the shared utils module:

```python
import hashlib


def derive_device_id(sidewalk_uuid: str) -> str:
    """
    Derive SideCharge short device ID from Sidewalk UUID.

    Format: SC- + first 8 hex chars of SHA-256(sidewalk_uuid)
    """
    digest = hashlib.sha256(sidewalk_uuid.encode("utf-8")).hexdigest()
    return f"SC-{digest[:8]}"
```

---

## 8. Installer API

### 8.1 v1.0: CLI Tool (No API Gateway)

For v1.0 with 1--10 devices, a REST API is over-engineered. The operator (Emily) manages devices via a CLI script that talks directly to DynamoDB using the AWS SDK.

**File**: `aws/device_registry_cli.py`

**Commands**:

```
# List all devices
python3 aws/device_registry_cli.py list

# Show one device
python3 aws/device_registry_cli.py show SC-a3f7c021

# Show one device by Sidewalk UUID
python3 aws/device_registry_cli.py show --sidewalk-id b319d001-6b08-4d88-b4ca-4d2d98a6d43c

# Set installer info
python3 aws/device_registry_cli.py install SC-a3f7c021 \
    --owner "Jane Smith" \
    --email "jane@example.com" \
    --address "1234 Oak St, Denver, CO 80202" \
    --installer "Bob's Electric" \
    --meter "M123456789"

# Set location (lat/lon)
python3 aws/device_registry_cli.py locate SC-a3f7c021 --lat 39.7392 --lon -104.9903

# Decommission
python3 aws/device_registry_cli.py decommission SC-a3f7c021

# Show fleet health
python3 aws/device_registry_cli.py health
```

The CLI uses `boto3` directly (same credentials as `terraform apply` and `ota_deploy.py`). No API Gateway, no Lambda, no authentication beyond the operator's AWS IAM credentials.

### 8.2 v2.0: REST API (API Gateway + Lambda)

When the installer base grows beyond the operator, a proper API is needed. Design sketch (not built in v1.0):

```
POST   /devices                    # Create device (factory provisioning)
GET    /devices/{device_id}        # Get device details
PATCH  /devices/{device_id}        # Update device (installer sets location, owner)
GET    /devices?status=active      # List devices by status
GET    /devices?installer=Bob      # List devices by installer
DELETE /devices/{device_id}        # Decommission (sets status=returned)
GET    /health                     # Fleet health summary
```

Authentication: API key for MVP, Cognito for production (installer accounts). The API Lambda reads/writes the same `sidecharge-device-registry` table.

### 8.3 Why Not an Installer App in v1.0

- No user-facing app is in v1.0 scope (PRD 1.2).
- An app requires hosting, auth, CI/CD, mobile compatibility -- all orthogonal to the device's core value.
- The CLI serves the operator, and the operator is the only user managing devices at this scale.
- The installer is an electrician, not a software user. Asking them to download an app to register a device adds friction that a sticker + phone call avoids.

---

## 9. Implementation Plan

### 9.1 Files to Create

| File | Purpose |
|------|---------|
| `aws/device_liveness_lambda.py` | Liveness checker Lambda |
| `aws/device_registry_cli.py` | CLI tool for device management |
| `aws/tests/test_device_registry.py` | Unit tests for derive_device_id, registry upsert, liveness |

### 9.2 Files to Modify

| File | Change |
|------|--------|
| `aws/sidewalk_utils.py` | Add `derive_device_id()` function |
| `aws/decode_evse_lambda.py` | Add `update_device_registry()` call in handler |
| `aws/terraform/main.tf` | Add DynamoDB table, liveness Lambda, IAM policies |
| `aws/terraform/variables.tf` | Add `device_registry_table_name` variable |
| `aws/terraform/outputs.tf` | Add registry table and liveness Lambda outputs |

### 9.3 Implementation Order

1. **`sidewalk_utils.py`**: Add `derive_device_id()`. Write unit test.
2. **`terraform/`**: Add DynamoDB table, variable, output. `terraform apply`.
3. **`decode_evse_lambda.py`**: Add registry upsert logic. Add env var. Deploy via `terraform apply`.
4. **Verify**: Confirm auto-registration on next device uplink.
5. **`device_registry_cli.py`**: Build CLI. Verify `list`, `show`, `install`.
6. **`device_liveness_lambda.py`**: Build liveness checker. Add to Terraform. Deploy.
7. **Tests**: Unit tests for all new code. Integration test: auto-registration + liveness cycle.

### 9.4 Rollback

All changes are additive. The registry table is new (no existing data to corrupt). The decode Lambda's registry write is wrapped in a try/except that logs and continues -- a registry failure never blocks telemetry processing. Terraform `destroy` on the new resources reverts cleanly.

---

## 10. Open Questions

1. **Device label printing**: How/when does the `SC-XXXXXXXX` ID get printed on the device label? This is a physical manufacturing step, not a software concern, but the ID must be derived before the label is printed. Likely during factory provisioning. Deferred to TASK-024 (commissioning process).

2. **Multi-device scheduler**: The charge scheduler currently targets a single hardcoded device. With the registry, it should iterate over active devices. This is a v1.1 change that pairs with TASK-037 (per-device utility config).

3. **PII handling**: The registry stores PII (`owner_name`, `owner_email`, `install_address`). TASK-038 (privacy policy) should define encryption-at-rest requirements (DynamoDB encryption is on by default with AWS-managed keys), access controls, and retention policy for decommissioned devices.

4. **Sidewalk UUID stability**: Is the `WirelessDeviceId` UUID permanent for the lifetime of the device, or can it change if the device is re-provisioned? If it can change, the `sidewalk-id-index` GSI needs to handle updates. Current assumption: the UUID is permanent once MFG credentials are flashed.
