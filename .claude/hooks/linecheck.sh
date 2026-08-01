#!/usr/bin/env bash
# PostToolUse (Edit|Write) hook: flag lines over 90 chars in the content just written.
# Scope: documentation files (.txt/.md) flag ALL lines; code files flag COMMENT lines only
# (block /* ... */ and trailing //). Pure code lines are left alone. Reports back via a
# decision:block reason so the violations get reflowed to <=90 (the indent counts).
input=$(cat)
f=$(printf '%s' "$input" | jq -r '.tool_input.file_path // empty')
case "$f" in
  *.txt|*.md|*.cpp|*.h|*.hpp|*.cc|*.c|*.mm|*.cxx) ;;
  *) exit 0 ;;
esac
case "$f" in
  *.txt|*.md) d=1 ;;
  *) d=0 ;;
esac
v=$(printf '%s' "$input" | jq -r '.tool_input.new_string // .tool_input.content // empty' | awk -v docs="$d" '
{
  len = length($0)
  c = 0
  if (inblock) c = 1
  if ($0 ~ /(^|[^:])\/\//) c = 1          # trailing // (skip https:// etc.)
  if ($0 ~ /\/\*/) c = 1                    # opens a block comment
  if ($0 ~ /\/\*/ && $0 !~ /\*\//) inblock = 1
  else if (inblock && $0 ~ /\*\//) inblock = 0
  if (docs == "1") c = 1                    # docs: every line counts
  if (len > 90 && c == 1) printf "  (%d) %s\n", len, $0
}')
if [ -n "$v" ]; then
  jq -n --arg v "$v" '{decision:"block", reason:("90-char limit exceeded in comments/docs you just wrote — reflow to <=90 (the indent counts toward the limit):\n" + $v)}'
fi
