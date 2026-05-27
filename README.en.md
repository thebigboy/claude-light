# Claude Light

[中文说明](README.md)

A tiny macOS floating traffic light for Claude Code.

Claude Light shows Claude Code's current working state as a floating, always-on-top traffic light:

- red: `idle`, Claude is idle or the current turn has finished
- yellow: `thinking`, Claude is reasoning, generating, or continuing after a tool batch
- fast yellow pulse: `awaiting_confirmation`, Claude is waiting for your permission approval
- green: `working`, Claude is using tools, running commands, or reading/writing files
- orange/red: `error`, the turn ended with an error

The yellow and green lights use a soft breathing effect. When Claude is waiting for permission approval, the app still shows the yellow light, but the pulse speed is 4x the normal yellow pulse to get your attention. You can drag the floating light, and the app remembers its position for the next launch. On startup, the red, yellow, and green lights flash through three quick cycles so you can find the light immediately.

The app does not need an HTTP server, WebSocket, or background daemon. Claude Code hooks write a local JSON state file, and the macOS app watches that file.

## How It Works

```text
Claude Code hook event
        |
        v
bin/claude-light-state
        |
        v
~/.claude-light/state.json
        |
        v
Floating macOS traffic light
```

## Requirements

- macOS 13 or later
- Xcode command line tools or Xcode
- Swift 6 compatible toolchain
- Claude Code with hooks enabled

Check Swift:

```sh
swift --version
```

## Install

Clone the project:

```sh
git clone https://github.com/thebigboy/claude-light.git
cd claude-light
```

Build it:

```sh
swift build
```

## Run the Floating Light

```sh
swift run claude-light
```

You should see a small vertical traffic light floating on your Mac screen.

Keep this process running while you use Claude Code. You can stop it with `Ctrl-C` in the terminal.

## Manual Test

In another terminal:

```sh
bin/claude-light-state thinking
bin/claude-light-state awaiting_confirmation
bin/claude-light-state working
bin/claude-light-state idle
```

The light should switch between normal yellow, fast-pulsing yellow, green, and red.

The state file is written to:

```text
~/.claude-light/state.json
```

Example:

```json
{
  "state": "working",
  "updated_at": "2026-05-27T02:00:25Z",
  "session_id": "",
  "cwd": "",
  "hook_event_name": ""
}
```

## Configure Claude Code Hooks

Claude Light ships with a sample Claude Code hook configuration:

```text
examples/claude-code-settings.json
```

Copy the `hooks` object from that file into one of these Claude Code settings files:

- user-level, applies globally: `~/.claude/settings.json`
- project-level, committed with a project: `.claude/settings.json`
- local project-level, private to your machine: `.claude/settings.local.json`

For a global setup, edit:

```sh
mkdir -p ~/.claude
open -e ~/.claude/settings.json
```

If the file does not exist yet, you can start from the sample:

```sh
cp examples/claude-code-settings.json ~/.claude/settings.json
```

If you already have Claude Code settings, merge only the `hooks` section instead of overwriting the file.

## Important: Update the Project Path in Hook Commands

The sample hook configuration uses the project path from the author's machine:

```sh
/Users/wangzhen/code/ai/claude-light/bin/claude-light-state <state>
```

This absolute command path must be written into your `~/.claude/settings.json`, but `/Users/wangzhen/code/ai/claude-light` must be replaced with the real directory where you cloned this repo.

For example, if you cloned the project to:

```text
/Users/alice/dev/claude-light
```

Then the command in your settings should be:

```sh
/Users/alice/dev/claude-light/bin/claude-light-state <state>
```

You can check the current project directory with:

```sh
pwd
```

Then append `/bin/claude-light-state` to that path and use it in every hook command in `~/.claude/settings.json`.

## Light Meanings

| Display | State value | Meaning |
| --- | --- | --- |
| Solid red | `idle` | Claude is idle, or the current turn has finished |
| Slow yellow pulse | `thinking` | Claude is reasoning, generating, or continuing after a tool batch |
| Fast yellow pulse | `awaiting_confirmation` | Claude is waiting for your permission approval; return to the terminal |
| Slow green pulse | `working` | Claude is using tools, such as running commands, reading files, editing files, or searching |
| Solid orange/red | `error` | The turn ended with an error, or the state file could not be parsed |

## Hook Mapping

| Claude Code event | Light state | Meaning |
| --- | --- | --- |
| `UserPromptSubmit` | `thinking` | Claude received your prompt |
| `PreToolUse` | `working` | Claude is about to use a tool |
| `PostToolBatch` | `thinking` | Tool batch finished, Claude may continue reasoning |
| `PermissionRequest` | `awaiting_confirmation` | Claude is about to show a permission prompt |
| `Notification: permission_prompt` | `awaiting_confirmation` | Claude is waiting for permission |
| `Stop` | `idle` | Claude finished the turn |
| `StopFailure` | `error` | The turn ended with an error |

## Sample Hook Configuration

See [examples/claude-code-settings.json](examples/claude-code-settings.json).

Each hook calls:

```sh
/Users/wangzhen/code/ai/claude-light/bin/claude-light-state <state>
```

If you clone the repo somewhere else, update those absolute paths in your Claude Code settings. In particular, replace `/Users/wangzhen/code/ai/claude-light` with your local clone directory.

## Troubleshooting

### The light does not change when running the script

Check the state file:

```sh
cat ~/.claude-light/state.json
```

Then try:

```sh
bin/claude-light-state working
```

### The app starts but logs IMK messages

Messages like these are macOS InputMethodKit logs and are usually harmless:

```text
+[IMKClient subclass]: chose IMKClient_Modern
error messaging the mach port for IMKCFRunLoopWakeUpReliable
```

If the light appears and changes state, you can ignore them.

### Claude Code hooks do not fire

Run `/hooks` inside Claude Code and verify that your hook configuration is loaded.

Also confirm the command path in your settings points to the real script location:

```sh
ls -l /Users/wangzhen/code/ai/claude-light/bin/claude-light-state
```

## Inspired By

The color and traffic-light feel were inspired by [JasonLam08/cursor_agent_status_light](https://github.com/JasonLam08/cursor_agent_status_light/), an ESP32-C3 BLE status light for Cursor Agent.

## License

MIT
