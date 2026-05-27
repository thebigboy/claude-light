# Claude Light

[English](README.en.md)

Claude Light 是一个用于 Claude Code 的 macOS 悬浮红绿灯。

它会把 Claude Code 当前的工作状态显示成一个始终置顶的小交通灯：

- 红灯：`idle`，空闲或本轮已结束
- 黄灯：`thinking`，Claude 正在思考、生成回复，或工具执行结束后继续推理
- 黄灯快闪：`awaiting_confirmation`，Claude 正在等待你确认权限请求
- 绿灯：`working`，Claude 正在调用工具、执行命令或读写文件
- 橙红灯：`error`，任务异常结束

黄灯和绿灯点亮时带有柔和的呼吸效果。等待确认时仍显示黄灯，但呼吸速度会变为普通黄灯的 4 倍，用来提醒你回到 Claude Code 处理授权。悬浮灯可以拖动，关闭后下次启动会记住上次位置；刚启动时会按红、黄、绿快速依次亮三轮，方便你快速找到它在屏幕上的位置。

整个方案不需要 HTTP 服务、WebSocket 或额外守护进程；Claude Code hooks 只负责写入本地 JSON 状态文件，macOS 悬浮灯负责监听这个文件并刷新显示。

## 工作原理

```text
Claude Code hook 事件
        |
        v
bin/claude-light-state
        |
        v
~/.claude-light/state.json
        |
        v
macOS 悬浮红绿灯
```

## 环境要求

- macOS 13 或更高版本
- Xcode Command Line Tools 或 Xcode
- Swift 6 兼容工具链
- Claude Code，并启用 hooks

检查 Swift：

```sh
swift --version
```

## 安装

克隆项目：

```sh
git clone https://github.com/thebigboy/claude-light.git
cd claude-light
```

构建项目：

```sh
swift build
```

## 启动悬浮灯

```sh
swift run claude-light
```

启动后，你应该能在 Mac 屏幕上看到一个竖向排列的小红绿灯。

使用 Claude Code 时保持这个进程运行即可。如果需要停止，在终端按 `Ctrl-C`。

## 手动测试

打开另一个终端，执行：

```sh
bin/claude-light-state thinking
bin/claude-light-state awaiting_confirmation
bin/claude-light-state working
bin/claude-light-state idle
```

悬浮灯应该会依次切换为普通黄灯、快闪黄灯、绿灯、红灯。

状态文件会写入：

```text
~/.claude-light/state.json
```

示例：

```json
{
  "state": "working",
  "updated_at": "2026-05-27T02:00:25Z",
  "session_id": "",
  "cwd": "",
  "hook_event_name": ""
}
```

## 配置 Claude Code Hooks

项目内置了一份 Claude Code hooks 示例配置：

```text
examples/claude-code-settings.json
```

把这个文件里的 `hooks` 对象合并到下面任意一个 Claude Code 设置文件中：

- 用户级，全局生效：`~/.claude/settings.json`
- 项目级，可随项目提交：`.claude/settings.json`
- 项目本地级，仅当前机器生效：`.claude/settings.local.json`

如果你想全局生效，可以编辑：

```sh
mkdir -p ~/.claude
open -e ~/.claude/settings.json
```

如果文件还不存在，可以直接从示例复制：

```sh
cp examples/claude-code-settings.json ~/.claude/settings.json
```

如果你已经有 Claude Code 配置，请只合并 `hooks` 部分，避免覆盖原有设置。

## 重要：修改 Hook 命令里的项目路径

示例配置里的 hook 命令使用了当前作者机器上的项目路径：

```sh
/Users/wangzhen/code/ai/claude-light/bin/claude-light-state <state>
```

这个绝对路径必须写入你的 `~/.claude/settings.json`，但其中的 `/Users/wangzhen/code/ai/claude-light` 需要按你 `git clone` 后的实际项目目录修改。

例如，如果你把项目克隆到了：

```text
/Users/alice/dev/claude-light
```

那 settings 里的命令应该改成：

```sh
/Users/alice/dev/claude-light/bin/claude-light-state <state>
```

你可以用下面命令查看当前项目目录：

```sh
pwd
```

然后把输出路径拼上 `/bin/claude-light-state`，替换到 `~/.claude/settings.json` 的每个 hook command 中。

## 灯色含义

| 显示效果 | 状态值 | 含义 |
| --- | --- | --- |
| 红灯常亮 | `idle` | Claude 当前空闲，或本轮对话已经完成 |
| 黄灯慢呼吸 | `thinking` | Claude 正在思考、生成回复，或工具执行结束后继续推理 |
| 黄灯快闪 | `awaiting_confirmation` | Claude 正在等待你确认权限请求，需要回到终端处理 |
| 绿灯慢呼吸 | `working` | Claude 正在使用工具，例如执行命令、读取文件、编辑文件、搜索等 |
| 橙红灯常亮 | `error` | Claude 本轮任务异常结束，或状态文件无法解析 |

## Hook 状态映射

| Claude Code 事件 | 灯状态 | 含义 |
| --- | --- | --- |
| `UserPromptSubmit` | `thinking` | Claude 收到你的 prompt |
| `PreToolUse` | `working` | Claude 准备调用工具 |
| `PostToolBatch` | `thinking` | 工具批次完成，Claude 可能继续思考 |
| `PermissionRequest` | `awaiting_confirmation` | Claude 即将展示权限确认 |
| `Notification: permission_prompt` | `awaiting_confirmation` | Claude 正在等待用户授权 |
| `Stop` | `idle` | 本轮对话完成 |
| `StopFailure` | `error` | 本轮异常结束 |

## 示例 Hook 配置

参考 [examples/claude-code-settings.json](examples/claude-code-settings.json)。

每个 hook 都会调用：

```sh
/Users/wangzhen/code/ai/claude-light/bin/claude-light-state <state>
```

如果你把项目克隆到了其他目录，需要把 Claude Code 设置里的绝对路径改成你本机实际路径。尤其是 `/Users/wangzhen/code/ai/claude-light` 这一段，必须替换为你的 clone 目录。

## 常见问题

### 手动执行脚本后灯没有变化

先检查状态文件：

```sh
cat ~/.claude-light/state.json
```

再手动写入一个状态：

```sh
bin/claude-light-state working
```

### App 启动后终端出现 IMK 日志

类似下面的日志通常是 macOS InputMethodKit 的系统日志，可以忽略：

```text
+[IMKClient subclass]: chose IMKClient_Modern
error messaging the mach port for IMKCFRunLoopWakeUpReliable
```

只要悬浮灯能显示并跟随状态变化，就不影响使用。

### Claude Code hooks 没有触发

在 Claude Code 里运行 `/hooks`，确认配置已经被加载。

同时确认 settings 里的命令路径指向真实脚本：

```sh
ls -l /Users/wangzhen/code/ai/claude-light/bin/claude-light-state
```

## 灵感来源

配色和实体红绿灯的观感参考了 [JasonLam08/cursor_agent_status_light](https://github.com/JasonLam08/cursor_agent_status_light/)，这是一个基于 ESP32-C3 BLE 的 Cursor Agent 状态灯项目。

## 许可证

MIT
