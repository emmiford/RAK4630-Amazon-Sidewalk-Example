output "lambda_function_arn" {
  description = "ARN of the EVSE decoder Lambda function"
  value       = aws_lambda_function.uplink_decoder.arn
}

output "lambda_function_name" {
  description = "Name of the EVSE decoder Lambda function"
  value       = aws_lambda_function.uplink_decoder.function_name
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

output "charge_scheduler_lambda_arn" {
  description = "ARN of the charge scheduler Lambda function"
  value       = aws_lambda_function.charge_scheduler.arn
}

output "charge_scheduler_rule_arn" {
  description = "ARN of the charge scheduler EventBridge rule"
  value       = aws_cloudwatch_event_rule.charge_schedule.arn
}

output "ota_sender_lambda_arn" {
  description = "ARN of the OTA sender Lambda function"
  value       = aws_lambda_function.ota_sender.arn
}

output "ota_bucket_name" {
  description = "Name of the OTA firmware S3 bucket"
  value       = aws_s3_bucket.ota_firmware.id
}
