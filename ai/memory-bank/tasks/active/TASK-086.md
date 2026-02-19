# TASK-086: Terraform — add CMD_AUTH_KEY to charge scheduler Lambda

**Status**: not started
**Priority**: P2
**Owner**: Eliel
**Branch**: —
**Size**: S (1 point)

## Description
TASK-032 added HMAC-SHA256 command authentication to charge control downlinks, but the signing key (`CMD_AUTH_KEY`) is not yet in the Terraform config. The charge scheduler Lambda needs this environment variable to sign commands. Without it, `get_auth_key()` returns None and commands are sent unsigned (backward-compatible but unauthenticated).

## Dependencies
**Blocked by**: TASK-032 (merged)
**Blocks**: TASK-087

## Acceptance Criteria
- [ ] `CMD_AUTH_KEY` env var added to charge scheduler Lambda in `aws/terraform/`
- [ ] Key value sourced from AWS Secrets Manager or SSM Parameter Store (not hardcoded in .tf)
- [ ] `terraform plan` shows only the expected change
- [ ] After `terraform apply`, scheduler logs confirm signed payloads (payload hex length = 12 or 18, not 4 or 10)

## Testing Requirements
- [ ] `terraform plan` output reviewed
- [ ] Manual: trigger scheduler, verify payload hex in CloudWatch includes auth tag bytes

## Deliverables
- `aws/terraform/lambda.tf` (or equivalent): CMD_AUTH_KEY env var
- `aws/terraform/secrets.tf` (or equivalent): Secrets Manager resource for the key
