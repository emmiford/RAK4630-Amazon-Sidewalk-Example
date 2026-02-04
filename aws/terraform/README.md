# EVSE Monitor AWS Infrastructure

Terraform configuration for deploying the EVSE Sidewalk decoder Lambda and supporting AWS resources.

## Resources Created

- **Lambda Function**: Decodes EVSE sensor payloads from Sidewalk messages
- **DynamoDB Table**: Stores decoded telemetry events
- **IoT Wireless Destination**: Routes Sidewalk messages to the processing rule
- **IoT Topic Rule**: Triggers Lambda on incoming Sidewalk messages
- **IAM Roles/Policies**: Permissions for Lambda and IoT

## Prerequisites

1. AWS CLI configured with appropriate credentials
2. Terraform >= 1.0 installed
3. Sidewalk device provisioned in AWS IoT Wireless

## Usage

```bash
# Initialize Terraform
terraform init

# Review planned changes
terraform plan

# Apply changes
terraform apply

# Destroy resources (if needed)
terraform destroy
```

## Configuration

Edit `variables.tf` or create a `terraform.tfvars` file:

```hcl
aws_region           = "us-east-1"
environment          = "dev"
lambda_function_name = "evse-decoder"
dynamodb_table_name  = "sidewalk-v1-device_events_v2"
```

## Payload Formats

The Lambda supports two payload formats:

### New Raw Format (8 bytes)
```
Byte 0: Magic (0xE5)
Byte 1: Version (0x01)
Byte 2: J1772 state (0-6)
Byte 3-4: Pilot voltage mV (little-endian)
Byte 5-6: Current mA (little-endian)
Byte 7: Thermostat flags
```

### Legacy sid_demo Format
Variable-length wrapper with embedded EVSE payload (type 0x01).

## Connecting Sidewalk Device

After deploying, associate your Sidewalk device with the IoT Wireless destination:

```bash
aws iotwireless update-wireless-device \
  --id YOUR_DEVICE_ID \
  --destination-name evse-sidewalk-destination
```
