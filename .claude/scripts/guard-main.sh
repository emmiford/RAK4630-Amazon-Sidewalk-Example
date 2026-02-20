#!/usr/bin/env bash
# Claude Code PreToolUse hook: enforce worktree-only workflow for code changes.
# Input: JSON on stdin with tool_name, tool_input, cwd fields.
#
# Policy:
#   ON MAIN — allowed:  task/agent/config file edits, git ops, read-only commands, test runners
#   ON MAIN — blocked:  source code edits (app/, aws/, tests/, docs/), build/flash commands
#   IN WORKTREE — everything allowed

REPO_ROOT="/Users/emilyf/sidewalk-projects/rak-sid"

# Read stdin (hook input JSON)
INPUT=$(cat)

# Get the working directory from the hook input, fall back to PWD
HOOK_CWD=$(echo "$INPUT" | python3 -c "import sys,json; print(json.load(sys.stdin).get('cwd',''))" 2>/dev/null)
CWD="${HOOK_CWD:-$PWD}"

# Only guard if we are inside the rak-sid repo (not a worktree)
if [[ "$CWD" == "$REPO_ROOT"* ]] && [[ "$CWD" != *"/worktrees/"* ]]; then
  BRANCH=$(git -C "$CWD" rev-parse --abbrev-ref HEAD 2>/dev/null)
  if [[ "$BRANCH" == "main" ]]; then
    # Extract tool name, command, and file path from hook input
    TOOL_NAME=$(echo "$INPUT" | python3 -c "import sys,json; print(json.load(sys.stdin).get('tool_name',''))" 2>/dev/null)
    CMD=$(echo "$INPUT" | python3 -c "import sys,json; print(json.load(sys.stdin).get('tool_input',{}).get('command',''))" 2>/dev/null)
    FILE_PATH=$(echo "$INPUT" | python3 -c "import sys,json; print(json.load(sys.stdin).get('tool_input',{}).get('file_path',''))" 2>/dev/null)

    # --- Edit / Write: allow only task, agent, config, and CLAUDE.md files ---
    if [[ "$TOOL_NAME" == "Edit" ]] || [[ "$TOOL_NAME" == "Write" ]]; then
      if echo "$FILE_PATH" | grep -qE '/(ai/memory-bank|ai/agents|\.claude)/'; then
        exit 0
      fi
      if echo "$FILE_PATH" | grep -qE '/CLAUDE\.md$'; then
        exit 0
      fi
      # Block all other file edits on main
      python3 -c "
import json
print(json.dumps({
    'hookSpecificOutput': {
        'hookEventName': 'PreToolUse',
        'permissionDecision': 'deny',
        'permissionDecisionReason': 'BLOCKED: Cannot edit source/doc files on main. Create a worktree:\n  git worktree add ../worktrees/task-NNN -b task/NNN-short-slug main\n  cd ../worktrees/task-NNN\nEdit task files (ai/memory-bank/), agent defs (ai/agents/), and .claude/ config on main. Everything else needs a branch.'
    }
}))
"
      exit 0
    fi

    # --- Bash: allow git, read-only, test runners; block build/flash ---
    if [[ "$TOOL_NAME" == "Bash" ]]; then
      # Allow all git commands (with optional -C prefix)
      if echo "$CMD" | grep -qE '^git (-C [^ ]+ )?'; then
        exit 0
      fi
      # Allow read-only and scripting commands
      if echo "$CMD" | grep -qE '^(ls|cat|head|tail|find|grep|rg|wc|echo|python3 |cd |pwd|which|type|file |chmod|gh |terraform )'; then
        exit 0
      fi
      # Allow test runners
      if echo "$CMD" | grep -qE '^(python3 -m pytest|ctest )'; then
        exit 0
      fi
      # Block build/flash commands with specific message
      if echo "$CMD" | grep -qE '^(cmake|make|west |nrfutil |pyocd )'; then
        python3 -c "
import json
print(json.dumps({
    'hookSpecificOutput': {
        'hookEventName': 'PreToolUse',
        'permissionDecision': 'deny',
        'permissionDecisionReason': 'BLOCKED: Build/flash commands must run from a worktree, not main.\nBuild in the worktree to verify BEFORE merging:\n  cd ../worktrees/task-NNN && cmake ...'
    }
}))
"
        exit 0
      fi
      # Allow anything else not explicitly blocked (e.g. bash one-liners, misc tools)
      exit 0
    fi

    # --- NotebookEdit: block on main ---
    if [[ "$TOOL_NAME" == "NotebookEdit" ]]; then
      python3 -c "
import json
print(json.dumps({
    'hookSpecificOutput': {
        'hookEventName': 'PreToolUse',
        'permissionDecision': 'deny',
        'permissionDecisionReason': 'BLOCKED: Notebook edits must happen in a worktree, not on main.'
    }
}))
"
      exit 0
    fi

    # Default: allow (Read, Glob, Grep, etc. are fine on main)
    exit 0
  fi
fi

# Outside rak-sid or in a worktree — allow everything
exit 0
