output "lambda_function_arn" {
  description = "ARN of the EVSE decoder Lambda function"
  value       = aws_lambda_function.evse_decoder.arn
}

output "lambda_function_name" {
  description = "Name of the EVSE decoder Lambda function"
  value       = aws_lambda_function.evse_decoder.function_name
}

output "dynamodb_table_name" {
  description = "Name of the DynamoDB table"
  value       = aws_dynamodb_table.evse_events.name
}

output "dynamodb_table_arn" {
  description = "ARN of the DynamoDB table"
  value       = aws_dynamodb_table.evse_events.arn
}

output "iot_rule_name" {
  description = "Name of the IoT topic rule"
  value       = aws_iot_topic_rule.evse_rule.name
}

output "iot_rule_arn" {
  description = "ARN of the IoT topic rule"
  value       = aws_iot_topic_rule.evse_rule.arn
}
