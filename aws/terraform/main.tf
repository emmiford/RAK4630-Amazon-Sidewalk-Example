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

# Package Lambda function code
data "archive_file" "lambda_zip" {
  type        = "zip"
  source_file = "${path.module}/../decode_evse_lambda.py"
  output_path = "${path.module}/../decode_evse_lambda.zip"
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
      DYNAMODB_TABLE = var.dynamodb_table_name
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
  retention_in_days = 14
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
  sql         = "SELECT * FROM '$aws/rules/${var.iot_rule_name}'"
  sql_version = "2016-03-23"

  lambda {
    function_arn = aws_lambda_function.evse_decoder.arn
  }

  tags = {
    Project     = "evse-monitor"
    Environment = var.environment
  }
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
