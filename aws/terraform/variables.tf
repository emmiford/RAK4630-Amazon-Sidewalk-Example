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

variable "scheduler_rate_minutes" {
  description = "How often the charge scheduler runs (minutes). Use 5 for debug, 60 for production."
  type        = number
  default     = 5
}

variable "watttime_username" {
  description = "WattTime API username"
  type        = string
  sensitive   = true
  default     = ""
}

variable "watttime_password" {
  description = "WattTime API password"
  type        = string
  sensitive   = true
  default     = ""
}

variable "moer_threshold" {
  description = "MOER percent threshold above which charging is paused (0-100)"
  type        = number
  default     = 70
}

variable "ota_bucket_name" {
  description = "S3 bucket name for OTA firmware binaries"
  type        = string
  default     = "evse-ota-firmware-dev"
}

variable "device_registry_table_name" {
  description = "DynamoDB table name for the SideCharge device registry"
  type        = string
  default     = "sidecharge-device-registry"
}
