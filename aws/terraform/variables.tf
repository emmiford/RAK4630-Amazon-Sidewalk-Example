variable "aws_region" {
  description = "AWS region for resources"
  type        = string
  default     = "us-east-1"
}

variable "environment" {
  description = "Environment name (dev, staging, prod)"
  type        = string
  default     = "dev"
}

variable "lambda_function_name" {
  description = "Name of the Lambda function"
  type        = string
  default     = "evse-decoder"
}

variable "dynamodb_table_name" {
  description = "Name of the DynamoDB table for EVSE events"
  type        = string
  default     = "sidewalk-v1-device_events_v2"
}

variable "iot_destination_name" {
  description = "Name of the IoT Wireless destination"
  type        = string
  default     = "evse-sidewalk-destination"
}

variable "iot_rule_name" {
  description = "Name of the IoT topic rule"
  type        = string
  default     = "evse_sidewalk_rule"
}
