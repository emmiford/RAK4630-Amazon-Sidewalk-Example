terraform {
  required_version = ">= 1.0"

  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 5.0"
    }
    archive = {
      source  = "hashicorp/archive"
      version = "~> 2.0"
    }
  }
}

provider "aws" {
  region = var.aws_region
}

# Package decode Lambda (includes shared utils + device registry)
data "archive_file" "lambda_zip" {
  type        = "zip"
  output_path = "${path.module}/../decode_evse_lambda.zip"

  source {
    content  = file("${path.module}/../decode_evse_lambda.py")
    filename = "decode_evse_lambda.py"
  }
  source {
    content  = file("${path.module}/../sidewalk_utils.py")
    filename = "sidewalk_utils.py"
  }
  source {
    content  = file("${path.module}/../device_registry.py")
    filename = "device_registry.py"
  }
}

# IAM role for Lambda
resource "aws_iam_role" "evse_decoder_role" {
  name = "evse-decoder-lambda-role"

  assume_role_policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Action = "sts:AssumeRole"
        Effect = "Allow"
        Principal = {
          Service = "lambda.amazonaws.com"
        }
      }
    ]
  })
}

# IAM policy for Lambda to access DynamoDB and CloudWatch
resource "aws_iam_role_policy" "evse_decoder_policy" {
  name = "evse-decoder-lambda-policy"
  role = aws_iam_role.evse_decoder_role.id

  policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Effect = "Allow"
        Action = [
          "logs:CreateLogGroup",
          "logs:CreateLogStream",
          "logs:PutLogEvents"
        ]
        Resource = "arn:aws:logs:*:*:*"
      },
      {
        Effect = "Allow"
        Action = [
          "dynamodb:PutItem",
          "dynamodb:GetItem",
          "dynamodb:UpdateItem",
          "dynamodb:Query"
        ]
        Resource = "arn:aws:dynamodb:${var.aws_region}:*:table/${var.dynamodb_table_name}"
      },
      {
        Effect = "Allow"
        Action = [
          "dynamodb:PutItem",
          "dynamodb:GetItem",
          "dynamodb:UpdateItem",
          "dynamodb:Scan"
        ]
        Resource = aws_dynamodb_table.device_registry.arn
      },
      {
        Effect = "Allow"
        Action = [
          "lambda:InvokeFunction"
        ]
        Resource = [
          "arn:aws:lambda:${var.aws_region}:*:function:${aws_lambda_function.ota_sender.function_name}",
          "arn:aws:lambda:${var.aws_region}:*:function:${aws_lambda_function.charge_scheduler.function_name}"
        ]
      },
      {
        Effect = "Allow"
        Action = [
          "iotwireless:SendDataToWirelessDevice",
          "iotwireless:ListWirelessDevices"
        ]
        Resource = "*"
      }
    ]
  })
}

# Lambda function
resource "aws_lambda_function" "evse_decoder" {
  filename         = data.archive_file.lambda_zip.output_path
  function_name    = var.lambda_function_name
  role             = aws_iam_role.evse_decoder_role.arn
  handler          = "decode_evse_lambda.lambda_handler"
  source_code_hash = data.archive_file.lambda_zip.output_base64sha256
  runtime          = "python3.11"
  timeout          = 30
  memory_size      = 128

  environment {
    variables = {
      DYNAMODB_TABLE         = var.dynamodb_table_name
      OTA_LAMBDA_NAME        = aws_lambda_function.ota_sender.function_name
      SCHEDULER_LAMBDA_NAME  = aws_lambda_function.charge_scheduler.function_name
      DEVICE_REGISTRY_TABLE  = var.device_registry_table_name
    }
  }

  tags = {
    Project     = "evse-monitor"
    Environment = var.environment
  }
}

# CloudWatch Log Group for Lambda
resource "aws_cloudwatch_log_group" "evse_decoder_logs" {
  name              = "/aws/lambda/${var.lambda_function_name}"
  retention_in_days = 30
}

# Permission for IoT to invoke Lambda
resource "aws_lambda_permission" "iot_invoke" {
  statement_id  = "AllowIoTInvoke"
  action        = "lambda:InvokeFunction"
  function_name = aws_lambda_function.evse_decoder.function_name
  principal     = "iot.amazonaws.com"
}

# IoT Rule to route Sidewalk messages to Lambda
resource "aws_iot_topic_rule" "evse_rule" {
  name        = var.iot_rule_name
  enabled     = true
  sql         = "SELECT * FROM 'sidewalk/#'"
  sql_version = "2016-03-23"

  lambda {
    function_arn = aws_lambda_function.evse_decoder.arn
  }

  tags = {
    Project     = "evse-monitor"
    Environment = var.environment
  }
}

# --- Charge Scheduler Lambda ---

# Package scheduler Lambda (includes shared utils)
data "archive_file" "scheduler_zip" {
  type        = "zip"
  output_path = "${path.module}/../charge_scheduler_lambda.zip"

  source {
    content  = file("${path.module}/../charge_scheduler_lambda.py")
    filename = "charge_scheduler_lambda.py"
  }
  source {
    content  = file("${path.module}/../sidewalk_utils.py")
    filename = "sidewalk_utils.py"
  }
}

# IAM role for scheduler Lambda
resource "aws_iam_role" "charge_scheduler_role" {
  name = "charge-scheduler-lambda-role"

  assume_role_policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Action = "sts:AssumeRole"
        Effect = "Allow"
        Principal = {
          Service = "lambda.amazonaws.com"
        }
      }
    ]
  })
}

# IAM policy: CloudWatch + DynamoDB + IoT Wireless downlink
resource "aws_iam_role_policy" "charge_scheduler_policy" {
  name = "charge-scheduler-lambda-policy"
  role = aws_iam_role.charge_scheduler_role.id

  policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Effect = "Allow"
        Action = [
          "logs:CreateLogGroup",
          "logs:CreateLogStream",
          "logs:PutLogEvents"
        ]
        Resource = "arn:aws:logs:*:*:*"
      },
      {
        Effect = "Allow"
        Action = [
          "dynamodb:PutItem",
          "dynamodb:GetItem",
          "dynamodb:Query"
        ]
        Resource = "arn:aws:dynamodb:${var.aws_region}:*:table/${var.dynamodb_table_name}"
      },
      {
        Effect = "Allow"
        Action = [
          "iotwireless:SendDataToWirelessDevice",
          "iotwireless:ListWirelessDevices"
        ]
        Resource = "*"
      }
    ]
  })
}

# Scheduler Lambda function
resource "aws_lambda_function" "charge_scheduler" {
  filename         = data.archive_file.scheduler_zip.output_path
  function_name    = "charge-scheduler"
  role             = aws_iam_role.charge_scheduler_role.arn
  handler          = "charge_scheduler_lambda.lambda_handler"
  source_code_hash = data.archive_file.scheduler_zip.output_base64sha256
  runtime          = "python3.11"
  timeout          = 30
  memory_size      = 128

  environment {
    variables = {
      DYNAMODB_TABLE    = var.dynamodb_table_name
      WATTTIME_USERNAME = var.watttime_username
      WATTTIME_PASSWORD = var.watttime_password
      MOER_THRESHOLD    = tostring(var.moer_threshold)
    }
  }

  tags = {
    Project     = "evse-monitor"
    Environment = var.environment
  }
}

# CloudWatch Log Group for scheduler
resource "aws_cloudwatch_log_group" "charge_scheduler_logs" {
  name              = "/aws/lambda/charge-scheduler"
  retention_in_days = 30
}

# EventBridge rule — periodic schedule
resource "aws_cloudwatch_event_rule" "charge_schedule" {
  name                = "charge-scheduler-rule"
  schedule_expression = "rate(${var.scheduler_rate_minutes} ${var.scheduler_rate_minutes == 1 ? "minute" : "minutes"})"

  tags = {
    Project     = "evse-monitor"
    Environment = var.environment
  }
}

# EventBridge target → Lambda
resource "aws_cloudwatch_event_target" "charge_scheduler_target" {
  rule      = aws_cloudwatch_event_rule.charge_schedule.name
  target_id = "charge-scheduler-lambda"
  arn       = aws_lambda_function.charge_scheduler.arn
}

# Permission for EventBridge to invoke the scheduler Lambda
resource "aws_lambda_permission" "eventbridge_invoke" {
  statement_id  = "AllowEventBridgeInvoke"
  action        = "lambda:InvokeFunction"
  function_name = aws_lambda_function.charge_scheduler.function_name
  principal     = "events.amazonaws.com"
  source_arn    = aws_cloudwatch_event_rule.charge_schedule.arn
}

# DynamoDB table for storing EVSE events
resource "aws_dynamodb_table" "evse_events" {
  name         = var.dynamodb_table_name
  billing_mode = "PAY_PER_REQUEST"
  hash_key     = "device_id"
  range_key    = "timestamp"

  attribute {
    name = "device_id"
    type = "S"
  }

  attribute {
    name = "timestamp"
    type = "N"
  }

  ttl {
    attribute_name = "ttl"
    enabled        = true
  }

  tags = {
    Project     = "evse-monitor"
    Environment = var.environment
  }
}

# --- OTA Sender Lambda ---

# S3 bucket for firmware binaries
resource "aws_s3_bucket" "ota_firmware" {
  bucket = var.ota_bucket_name

  tags = {
    Project     = "evse-monitor"
    Environment = var.environment
  }
}

# Package OTA sender Lambda (includes shared utils)
data "archive_file" "ota_sender_zip" {
  type        = "zip"
  output_path = "${path.module}/../ota_sender_lambda.zip"

  source {
    content  = file("${path.module}/../ota_sender_lambda.py")
    filename = "ota_sender_lambda.py"
  }
  source {
    content  = file("${path.module}/../sidewalk_utils.py")
    filename = "sidewalk_utils.py"
  }
}

# IAM role for OTA sender Lambda
resource "aws_iam_role" "ota_sender_role" {
  name = "ota-sender-lambda-role"

  assume_role_policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Action = "sts:AssumeRole"
        Effect = "Allow"
        Principal = {
          Service = "lambda.amazonaws.com"
        }
      }
    ]
  })
}

# IAM policy: CloudWatch + S3 + DynamoDB + IoT Wireless
resource "aws_iam_role_policy" "ota_sender_policy" {
  name = "ota-sender-lambda-policy"
  role = aws_iam_role.ota_sender_role.id

  policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Effect = "Allow"
        Action = [
          "logs:CreateLogGroup",
          "logs:CreateLogStream",
          "logs:PutLogEvents"
        ]
        Resource = "arn:aws:logs:*:*:*"
      },
      {
        Effect = "Allow"
        Action = [
          "s3:GetObject",
          "s3:PutObject"
        ]
        Resource = "${aws_s3_bucket.ota_firmware.arn}/*"
      },
      {
        Effect = "Allow"
        Action = [
          "dynamodb:PutItem",
          "dynamodb:GetItem",
          "dynamodb:DeleteItem",
          "dynamodb:Query"
        ]
        Resource = "arn:aws:dynamodb:${var.aws_region}:*:table/${var.dynamodb_table_name}"
      },
      {
        Effect = "Allow"
        Action = [
          "iotwireless:SendDataToWirelessDevice",
          "iotwireless:ListWirelessDevices"
        ]
        Resource = "*"
      }
    ]
  })
}

# OTA sender Lambda function
resource "aws_lambda_function" "ota_sender" {
  filename         = data.archive_file.ota_sender_zip.output_path
  function_name    = "ota-sender"
  role             = aws_iam_role.ota_sender_role.arn
  handler          = "ota_sender_lambda.lambda_handler"
  source_code_hash = data.archive_file.ota_sender_zip.output_base64sha256
  runtime          = "python3.11"
  timeout          = 60
  memory_size      = 128

  environment {
    variables = {
      DYNAMODB_TABLE = var.dynamodb_table_name
      OTA_BUCKET     = var.ota_bucket_name
    }
  }

  tags = {
    Project     = "evse-monitor"
    Environment = var.environment
  }
}

# CloudWatch Log Group for OTA sender
resource "aws_cloudwatch_log_group" "ota_sender_logs" {
  name              = "/aws/lambda/ota-sender"
  retention_in_days = 30
}

# Permission for S3 to invoke OTA sender Lambda
resource "aws_lambda_permission" "s3_invoke_ota" {
  statement_id  = "AllowS3Invoke"
  action        = "lambda:InvokeFunction"
  function_name = aws_lambda_function.ota_sender.function_name
  principal     = "s3.amazonaws.com"
  source_arn    = aws_s3_bucket.ota_firmware.arn
}

# Permission for decode Lambda to invoke OTA sender Lambda
resource "aws_lambda_permission" "decode_invoke_ota" {
  statement_id  = "AllowDecodeInvoke"
  action        = "lambda:InvokeFunction"
  function_name = aws_lambda_function.ota_sender.function_name
  principal     = "lambda.amazonaws.com"
  source_arn    = aws_lambda_function.evse_decoder.arn
}

# S3 bucket notification → OTA sender Lambda
resource "aws_s3_bucket_notification" "ota_upload" {
  bucket = aws_s3_bucket.ota_firmware.id

  lambda_function {
    lambda_function_arn = aws_lambda_function.ota_sender.arn
    events              = ["s3:ObjectCreated:*"]
    filter_prefix       = "firmware/"
    filter_suffix       = ".bin"
  }

  depends_on = [aws_lambda_permission.s3_invoke_ota]
}

# EventBridge rule — OTA retry timer (1 minute)
resource "aws_cloudwatch_event_rule" "ota_retry" {
  name                = "ota-retry-timer"
  state               = "ENABLED"
  schedule_expression = "rate(1 minute)"

  tags = {
    Project     = "evse-monitor"
    Environment = var.environment
  }
}

# EventBridge target → OTA sender Lambda
resource "aws_cloudwatch_event_target" "ota_retry_target" {
  rule      = aws_cloudwatch_event_rule.ota_retry.name
  target_id = "ota-sender-retry"
  arn       = aws_lambda_function.ota_sender.arn
}

# Permission for EventBridge to invoke OTA sender
resource "aws_lambda_permission" "eventbridge_invoke_ota" {
  statement_id  = "AllowEventBridgeInvokeOTA"
  action        = "lambda:InvokeFunction"
  function_name = aws_lambda_function.ota_sender.function_name
  principal     = "events.amazonaws.com"
  source_arn    = aws_cloudwatch_event_rule.ota_retry.arn
}

# --- OTA Stale Session Alarm ---
# Fires if ota-sender Lambda has errors for 10+ minutes,
# indicating the retry timer may be disabled or broken.

resource "aws_cloudwatch_metric_alarm" "ota_sender_errors" {
  alarm_name          = "ota-sender-errors"
  alarm_description   = "OTA sender Lambda errors detected — retry timer may be disabled or session stalled"
  namespace           = "AWS/Lambda"
  metric_name         = "Errors"
  statistic           = "Sum"
  period              = 600
  evaluation_periods  = 1
  threshold           = 1
  comparison_operator = "GreaterThanOrEqualToThreshold"
  treat_missing_data  = "notBreaching"
  actions_enabled     = var.alarms_enabled
  alarm_actions       = [aws_sns_topic.alerts.arn]
  ok_actions          = [aws_sns_topic.alerts.arn]

  dimensions = {
    FunctionName = aws_lambda_function.ota_sender.function_name
  }

  tags = {
    Project     = "evse-monitor"
    Environment = var.environment
  }
}

resource "aws_cloudwatch_metric_alarm" "ota_retry_not_firing" {
  alarm_name          = "ota-retry-not-firing"
  alarm_description   = "OTA retry timer has not invoked ota-sender in 10 min — rule may be disabled"
  namespace           = "AWS/Lambda"
  metric_name         = "Invocations"
  statistic           = "Sum"
  period              = 600
  evaluation_periods  = 1
  threshold           = 1
  comparison_operator = "LessThanThreshold"
  treat_missing_data  = "breaching"
  actions_enabled     = var.alarms_enabled
  alarm_actions       = [aws_sns_topic.alerts.arn]
  ok_actions          = [aws_sns_topic.alerts.arn]

  dimensions = {
    FunctionName = aws_lambda_function.ota_sender.function_name
  }

  tags = {
    Project     = "evse-monitor"
    Environment = var.environment
  }
}

# --- Production Observability (TASK-029 Tier 1) ---

# SNS topic for all operational alerts
resource "aws_sns_topic" "alerts" {
  name = "sidecharge-alerts"

  tags = {
    Project     = "evse-monitor"
    Environment = var.environment
  }
}

# Email subscription (operator notification)
resource "aws_sns_topic_subscription" "alert_email" {
  count     = var.alert_email != "" ? 1 : 0
  topic_arn = aws_sns_topic.alerts.arn
  protocol  = "email"
  endpoint  = var.alert_email
}

# --- Device Offline Detection ---
# Metric filter: count uplinks per device from decode Lambda logs.
# The decode Lambda logs "Stored decoded EVSE data" on every successful write.
# Alarm fires when no uplink for 2x heartbeat (30 min at 15-min heartbeat).

resource "aws_cloudwatch_log_metric_filter" "device_uplink" {
  name           = "device-uplink-count"
  log_group_name = aws_cloudwatch_log_group.evse_decoder_logs.name
  pattern        = "\"Stored decoded EVSE data\""

  metric_transformation {
    name          = "DeviceUplinkCount"
    namespace     = "SideCharge"
    value         = "1"
    default_value = "0"
  }
}

resource "aws_cloudwatch_metric_alarm" "device_offline" {
  alarm_name          = "device-offline"
  alarm_description   = "No device uplink received for 30 min (2x heartbeat). Device may be offline or out of range."
  namespace           = "SideCharge"
  metric_name         = "DeviceUplinkCount"
  statistic           = "Sum"
  period              = 1800  # 30 minutes (2x 15-min heartbeat)
  evaluation_periods  = 1
  threshold           = 1
  comparison_operator = "LessThanThreshold"
  treat_missing_data  = "breaching"
  actions_enabled     = var.alarms_enabled
  alarm_actions       = [aws_sns_topic.alerts.arn]
  ok_actions          = [aws_sns_topic.alerts.arn]

  tags = {
    Project     = "evse-monitor"
    Environment = var.environment
  }
}

# --- Interlock State Change Logging ---
# Metric filter: count interlock activations (cool_call transitions) from
# decode Lambda logs. The Lambda logs thermostat_cool_active for every uplink.

resource "aws_cloudwatch_log_metric_filter" "interlock_activation" {
  name           = "interlock-activation-count"
  log_group_name = aws_cloudwatch_log_group.evse_decoder_logs.name
  pattern        = "{ $.thermostat_cool = true }"

  metric_transformation {
    name          = "InterlockActivationCount"
    namespace     = "SideCharge"
    value         = "1"
    default_value = "0"
  }
}

# --- Scheduler Divergence Alarm (TASK-071) ---
# Fires when the decode Lambda logs DIVERGENCE_RETRIES_EXHAUSTED,
# meaning the device has not acknowledged a scheduler command after 3 re-sends.

resource "aws_cloudwatch_log_metric_filter" "divergence_exhausted" {
  name           = "divergence-retries-exhausted"
  log_group_name = aws_cloudwatch_log_group.evse_decoder_logs.name
  pattern        = "\"DIVERGENCE_RETRIES_EXHAUSTED\""

  metric_transformation {
    name          = "DivergenceRetriesExhausted"
    namespace     = "SideCharge"
    value         = "1"
    default_value = "0"
  }
}

resource "aws_cloudwatch_metric_alarm" "divergence_exhausted" {
  alarm_name          = "scheduler-divergence-exhausted"
  alarm_description   = "Device has not acknowledged scheduler command after 3 re-sends. LoRa link may be down."
  namespace           = "SideCharge"
  metric_name         = "DivergenceRetriesExhausted"
  statistic           = "Sum"
  period              = 3600  # 1 hour
  evaluation_periods  = 1
  threshold           = 1
  comparison_operator = "GreaterThanOrEqualToThreshold"
  treat_missing_data  = "notBreaching"
  actions_enabled     = var.alarms_enabled
  alarm_actions       = [aws_sns_topic.alerts.arn]
  ok_actions          = [aws_sns_topic.alerts.arn]

  tags = {
    Project     = "evse-monitor"
    Environment = var.environment
  }
}

# --- Health Digest Lambda ---

# Package health digest Lambda
data "archive_file" "health_digest_zip" {
  type        = "zip"
  output_path = "${path.module}/../health_digest_lambda.zip"

  source {
    content  = file("${path.module}/../health_digest_lambda.py")
    filename = "health_digest_lambda.py"
  }
  source {
    content  = file("${path.module}/../device_registry.py")
    filename = "device_registry.py"
  }
}

# IAM role for health digest Lambda
resource "aws_iam_role" "health_digest_role" {
  name = "health-digest-lambda-role"

  assume_role_policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Action = "sts:AssumeRole"
        Effect = "Allow"
        Principal = {
          Service = "lambda.amazonaws.com"
        }
      }
    ]
  })
}

# IAM policy: CloudWatch logs + DynamoDB read + SNS publish
resource "aws_iam_role_policy" "health_digest_policy" {
  name = "health-digest-lambda-policy"
  role = aws_iam_role.health_digest_role.id

  policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Effect = "Allow"
        Action = [
          "logs:CreateLogGroup",
          "logs:CreateLogStream",
          "logs:PutLogEvents"
        ]
        Resource = "arn:aws:logs:*:*:*"
      },
      {
        Effect = "Allow"
        Action = [
          "dynamodb:Scan",
          "dynamodb:Query",
          "dynamodb:GetItem"
        ]
        Resource = [
          aws_dynamodb_table.device_registry.arn,
          "arn:aws:dynamodb:${var.aws_region}:*:table/${var.dynamodb_table_name}"
        ]
      },
      {
        Effect = "Allow"
        Action = [
          "sns:Publish"
        ]
        Resource = aws_sns_topic.alerts.arn
      }
    ]
  })
}

# Health digest Lambda function
resource "aws_lambda_function" "health_digest" {
  filename         = data.archive_file.health_digest_zip.output_path
  function_name    = "health-digest"
  role             = aws_iam_role.health_digest_role.arn
  handler          = "health_digest_lambda.lambda_handler"
  source_code_hash = data.archive_file.health_digest_zip.output_base64sha256
  runtime          = "python3.11"
  timeout          = 60
  memory_size      = 128

  environment {
    variables = {
      DEVICE_REGISTRY_TABLE = var.device_registry_table_name
      DYNAMODB_TABLE        = var.dynamodb_table_name
      SNS_TOPIC_ARN         = aws_sns_topic.alerts.arn
      HEARTBEAT_INTERVAL_S  = tostring(var.heartbeat_interval_s)
    }
  }

  tags = {
    Project     = "evse-monitor"
    Environment = var.environment
  }
}

# CloudWatch Log Group for health digest
resource "aws_cloudwatch_log_group" "health_digest_logs" {
  name              = "/aws/lambda/health-digest"
  retention_in_days = 30
}

# EventBridge rule — daily at 08:00 UTC
resource "aws_cloudwatch_event_rule" "health_digest_schedule" {
  name                = "health-digest-daily"
  schedule_expression = "cron(0 8 * * ? *)"

  tags = {
    Project     = "evse-monitor"
    Environment = var.environment
  }
}

# EventBridge target → health digest Lambda
resource "aws_cloudwatch_event_target" "health_digest_target" {
  rule      = aws_cloudwatch_event_rule.health_digest_schedule.name
  target_id = "health-digest-lambda"
  arn       = aws_lambda_function.health_digest.arn
}

# Permission for EventBridge to invoke health digest Lambda
resource "aws_lambda_permission" "eventbridge_invoke_health_digest" {
  statement_id  = "AllowEventBridgeInvokeHealthDigest"
  action        = "lambda:InvokeFunction"
  function_name = aws_lambda_function.health_digest.function_name
  principal     = "events.amazonaws.com"
  source_arn    = aws_cloudwatch_event_rule.health_digest_schedule.arn
}
