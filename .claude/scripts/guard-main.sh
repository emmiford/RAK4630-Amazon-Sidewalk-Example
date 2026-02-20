#!/usr/bin/env bash
# Claude Code PreToolUse hook: block file modifications on main branch.
# Input: JSON on stdin with tool_name and tool_input fields.
# If working directory is inside rak-sid/ and branch is "main", reject the tool call.

REPO_ROOT="/Users/emilyf/sidewalk-projects/rak-sid"

# Read stdin (hook input JSON)
INPUT=$(cat)

# Get the working directory from the hook input, fall back to PWD
HOOK_CWD=$(echo "$INPUT" | python3 -c "import sys,json; print(json.load(sys.stdin).get('cwd',''))" 2>/dev/null)
CWD="${HOOK_CWD:-$PWD}"

# Only guard if we're inside the rak-sid repo (not a worktree)
if [[ "$CWD" == "$REPO_ROOT"* ]] && [[ "$CWD" != *"/worktrees/"* ]]; then
  BRANCH=$(git -C "$CWD" rev-parse --abbrev-ref HEAD 2>/dev/null)
  if [[ "$BRANCH" == "main" ]]; then
    # Extract tool name and command
    TOOL_NAME=$(echo "$INPUT" | python3 -c "import sys,json; print(json.load(sys.stdin).get('tool_name',''))" 2>/dev/null)
    CMD=$(echo "$INPUT" | python3 -c "import sys,json; print(json.load(sys.stdin).get('tool_input',{}).get('command',''))" 2>/dev/null)

    # For Bash: allow git and read-only commands (needed to create worktrees/branches)
    if [[ "$TOOL_NAME" == "Bash" ]]; then
      if echo "$CMD" | grep -qE '^git (-C [^ ]+ )?(worktree|branch|checkout|switch|log|status|diff|remote|fetch|rev-parse|show-current|stash|add|commit|merge|push|mv|rm|tag)'; then
        exit 0
      fi
      if echo "$CMD" | grep -qE '^(ls|cat|head|tail|find|grep|rg|wc|echo|python3 -c|cd |pwd|which|type|file )'; then
        exit 0
      fi
      # Allow cd commands
      if echo "$CMD" | grep -qE '^cd '; then
        exit 0
      fi
    fi

    # Block everything else with a deny decision
    python3 -c "
import json
print(json.dumps({
    'hookSpecificOutput': {
        'hookEventName': 'PreToolUse',
        'permissionDecision': 'deny',
        'permissionDecisionReason': 'BLOCKED: You are on main in rak-sid/. Create a worktree first:\\n  cd /Users/emilyf/sidewalk-projects/rak-sid\\n  git worktree add ../worktrees/task-NNN -b task/NNN-short-slug main\\n  cd ../worktrees/task-NNN\\nThen work from the worktree. main is READ-ONLY.'
    }
}))
"
    exit 0
  fi
fi

exit 0
