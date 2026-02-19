# --- Daily Aggregation Lambda (TASK-078) ---
# Computes per-device daily summaries from raw telemetry.
# Runs daily at 02:00 UTC via EventBridge, writes to sidecharge-daily-aggregates.

# DynamoDB table for daily aggregates (3-year TTL)
resource "aws_dynamodb_table" "daily_aggregates" {
  name         = var.aggregates_table_name
  billing_mode = "PAY_PER_REQUEST"
  hash_key     = "device_id"
  range_key    = "date"

  attribute {
    name = "device_id"
    type = "S"
  }

  attribute {
    name = "date"
    type = "S"
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

# Package Lambda code
data "archive_file" "aggregation_zip" {
  type        = "zip"
  output_path = "${path.module}/../aggregation_lambda.zip"

  source {
    content  = file("${path.module}/../aggregation_lambda.py")
    filename = "aggregation_lambda.py"
  }
}

# IAM role
resource "aws_iam_role" "aggregation_role" {
  name = "daily-aggregation-lambda-role"

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

# IAM policy
resource "aws_iam_role_policy" "aggregation_policy" {
  name = "daily-aggregation-lambda-policy"
  role = aws_iam_role.aggregation_role.id

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
          "dynamodb:Scan"
        ]
        Resource = aws_dynamodb_table.device_registry.arn
      },
      {
        Effect = "Allow"
        Action = [
          "dynamodb:Query"
        ]
        Resource = "arn:aws:dynamodb:${var.aws_region}:*:table/${var.dynamodb_table_name}"
      },
      {
        Effect = "Allow"
        Action = [
          "dynamodb:PutItem"
        ]
        Resource = aws_dynamodb_table.daily_aggregates.arn
      }
    ]
  })
}

# Lambda function
resource "aws_lambda_function" "daily_aggregation" {
  filename         = data.archive_file.aggregation_zip.output_path
  function_name    = "daily-aggregation"
  role             = aws_iam_role.aggregation_role.arn
  handler          = "aggregation_lambda.lambda_handler"
  source_code_hash = data.archive_file.aggregation_zip.output_base64sha256
  runtime          = "python3.11"
  timeout          = 120
  memory_size      = 128

  environment {
    variables = {
      DYNAMODB_TABLE        = var.dynamodb_table_name
      DEVICE_REGISTRY_TABLE = var.device_registry_table_name
      AGGREGATES_TABLE      = var.aggregates_table_name
    }
  }

  tags = {
    Project     = "evse-monitor"
    Environment = var.environment
  }
}

# CloudWatch Log Group
resource "aws_cloudwatch_log_group" "aggregation_logs" {
  name              = "/aws/lambda/daily-aggregation"
  retention_in_days = 30
}

# EventBridge rule — daily at 02:00 UTC
resource "aws_cloudwatch_event_rule" "aggregation_schedule" {
  name                = "daily-aggregation-rule"
  schedule_expression = "cron(0 2 * * ? *)"

  tags = {
    Project     = "evse-monitor"
    Environment = var.environment
  }
}

# EventBridge target → Lambda
resource "aws_cloudwatch_event_target" "aggregation_target" {
  rule      = aws_cloudwatch_event_rule.aggregation_schedule.name
  target_id = "daily-aggregation-lambda"
  arn       = aws_lambda_function.daily_aggregation.arn
}

# Permission for EventBridge to invoke the aggregation Lambda
resource "aws_lambda_permission" "eventbridge_invoke_aggregation" {
  statement_id  = "AllowEventBridgeInvokeAggregation"
  action        = "lambda:InvokeFunction"
  function_name = aws_lambda_function.daily_aggregation.function_name
  principal     = "events.amazonaws.com"
  source_arn    = aws_cloudwatch_event_rule.aggregation_schedule.arn
}
