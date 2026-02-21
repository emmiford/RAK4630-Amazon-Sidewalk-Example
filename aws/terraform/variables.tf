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
  default     = "uplink-decoder"
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
  description = "DynamoDB table name for the EVSE Monitor device registry"
  type        = string
  default     = "device-registry"
}

variable "alert_email" {
  description = "Email address for operational alerts (leave empty to skip subscription)"
  type        = string
  default     = ""
}

variable "alarms_enabled" {
  description = "Whether CloudWatch alarm actions are enabled. Set false during development to avoid noise."
  type        = bool
  default     = false
}

variable "heartbeat_interval_s" {
  description = "Device heartbeat interval in seconds. 60 for dev, 900 for production."
  type        = number
  default     = 900
}

variable "auto_diag_enabled" {
  description = "Enable automatic 0x40 diagnostic queries to unhealthy devices in health digest."
  type        = string
  default     = "false"
}

variable "latest_app_version" {
  description = "Latest deployed app firmware version. Devices below this are flagged as stale. 0 = skip check."
  type        = number
  default     = 0
}

variable "aggregates_table_name" {
  description = "DynamoDB table name for daily aggregate summaries (TASK-078)"
  type        = string
  default     = "daily-aggregates"
}
