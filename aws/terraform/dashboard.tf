# --- EVSE Fleet Dashboard (TASK-106) ---
# Dashboard API Lambda + API Gateway HTTP API + S3 static site + CloudFront CDN.

# --- Lambda: dashboard_api ---

# Package dashboard API Lambda
data "archive_file" "dashboard_api_zip" {
  type        = "zip"
  output_path = "${path.module}/../dashboard_api_lambda.zip"

  source {
    content  = file("${path.module}/../dashboard_api_lambda.py")
    filename = "dashboard_api_lambda.py"
  }
  source {
    content  = file("${path.module}/../device_registry.py")
    filename = "device_registry.py"
  }
  source {
    content  = file("${path.module}/../protocol_constants.py")
    filename = "protocol_constants.py"
  }
}

# IAM role for dashboard API Lambda
resource "aws_iam_role" "dashboard_api_role" {
  name = "dashboard-api-lambda-role"

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

# IAM policy: CloudWatch logs + read-only DynamoDB on 4 tables
resource "aws_iam_role_policy" "dashboard_api_policy" {
  name = "dashboard-api-lambda-policy"
  role = aws_iam_role.dashboard_api_role.id

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
          "dynamodb:GetItem",
          "dynamodb:Query",
          "dynamodb:Scan"
        ]
        Resource = [
          "arn:aws:dynamodb:${var.aws_region}:*:table/${var.dynamodb_table_name}",
          "arn:aws:dynamodb:${var.aws_region}:*:table/${var.dynamodb_table_name}/index/event-type-index"
        ]
      },
      {
        Effect = "Allow"
        Action = [
          "dynamodb:GetItem",
          "dynamodb:Query",
          "dynamodb:Scan"
        ]
        Resource = aws_dynamodb_table.device_registry.arn
      },
      {
        Effect = "Allow"
        Action = [
          "dynamodb:GetItem",
          "dynamodb:Query",
          "dynamodb:Scan"
        ]
        Resource = aws_dynamodb_table.device_state.arn
      },
      {
        Effect = "Allow"
        Action = [
          "dynamodb:GetItem",
          "dynamodb:Query",
          "dynamodb:Scan"
        ]
        Resource = aws_dynamodb_table.daily_aggregates.arn
      }
    ]
  })
}

# Dashboard API Lambda function
resource "aws_lambda_function" "dashboard_api" {
  filename         = data.archive_file.dashboard_api_zip.output_path
  function_name    = "dashboard-api"
  role             = aws_iam_role.dashboard_api_role.arn
  handler          = "dashboard_api_lambda.lambda_handler"
  source_code_hash = data.archive_file.dashboard_api_zip.output_base64sha256
  runtime          = "python3.11"
  timeout          = 30
  memory_size      = 256

  environment {
    variables = {
      EVENTS_TABLE          = var.dynamodb_table_name
      DEVICE_REGISTRY_TABLE = var.device_registry_table_name
      DEVICE_STATE_TABLE    = var.device_state_table_name
      DAILY_STATS_TABLE     = var.aggregates_table_name
      DASHBOARD_API_KEY     = var.dashboard_api_key
    }
  }

  tags = {
    Project     = "evse-monitor"
    Environment = var.environment
  }
}

# CloudWatch Log Group for dashboard API
resource "aws_cloudwatch_log_group" "dashboard_api_logs" {
  name              = "/aws/lambda/dashboard-api"
  retention_in_days = 30
}

# --- API Gateway HTTP API (v2) ---

resource "aws_apigatewayv2_api" "dashboard" {
  name          = "evse-dashboard-api"
  protocol_type = "HTTP"

  cors_configuration {
    allow_origins = ["*"]
    allow_methods = ["GET", "POST", "OPTIONS"]
    allow_headers = ["Content-Type", "x-api-key"]
    max_age       = 3600
  }

  tags = {
    Project     = "evse-monitor"
    Environment = var.environment
  }
}

# Lambda integration
resource "aws_apigatewayv2_integration" "dashboard_lambda" {
  api_id                 = aws_apigatewayv2_api.dashboard.id
  integration_type       = "AWS_PROXY"
  integration_uri        = aws_lambda_function.dashboard_api.invoke_arn
  payload_format_version = "2.0"
}

# Default route → Lambda
resource "aws_apigatewayv2_route" "dashboard_default" {
  api_id    = aws_apigatewayv2_api.dashboard.id
  route_key = "$default"
  target    = "integrations/${aws_apigatewayv2_integration.dashboard_lambda.id}"
}

# Auto-deploy stage
resource "aws_apigatewayv2_stage" "dashboard_default" {
  api_id      = aws_apigatewayv2_api.dashboard.id
  name        = "$default"
  auto_deploy = true

  tags = {
    Project     = "evse-monitor"
    Environment = var.environment
  }
}

# Permission for API Gateway to invoke dashboard Lambda
resource "aws_lambda_permission" "apigw_invoke_dashboard" {
  statement_id  = "AllowAPIGatewayInvokeDashboard"
  action        = "lambda:InvokeFunction"
  function_name = aws_lambda_function.dashboard_api.function_name
  principal     = "apigateway.amazonaws.com"
  source_arn    = "${aws_apigatewayv2_api.dashboard.execution_arn}/*/*"
}

# --- Outputs ---

output "dashboard_api_url" {
  description = "API Gateway endpoint URL for the dashboard API"
  value       = aws_apigatewayv2_api.dashboard.api_endpoint
}
