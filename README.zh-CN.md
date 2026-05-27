# Claude Light

[English](README.md)

Claude Light 是一个用于 Claude Code 的 macOS 悬浮红绿灯。

它会把 Claude Code 当前的工作状态显示成一个始终置顶的小交通灯：

- 红灯：`idle`，空闲或本轮已结束
- 黄灯：`thinking`，Claude 正在思考或等待权限
- 绿灯：`working`，Claude 正在调用工具、执行命令或读写文件
- 橙红灯：`error`，任务异常结束

黄灯和绿灯点亮时带有柔和的呼吸效果。整个方案不需要 HTTP 服务、WebSocket 或额外守护进程；Claude Code hooks 只负责写入本地 JSON 状态文件，macOS 悬浮灯负责监听这个文件并刷新显示。

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
bin/claude-light-state working
bin/claude-light-state idle
```

悬浮灯应该会依次切换为黄灯、绿灯、红灯。

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

## Hook 状态映射

| Claude Code 事件 | 灯状态 | 含义 |
| --- | --- | --- |
| `UserPromptSubmit` | `thinking` | Claude 收到你的 prompt |
| `PreToolUse` | `working` | Claude 准备调用工具 |
| `PostToolBatch` | `thinking` | 工具批次完成，Claude 可能继续思考 |
| `Notification: permission_prompt` | `thinking` | Claude 正在等待用户授权 |
| `Stop` | `idle` | 本轮对话完成 |
| `StopFailure` | `error` | 本轮异常结束 |

## 示例 Hook 配置

参考 [examples/claude-code-settings.json](examples/claude-code-settings.json)。

每个 hook 都会调用：

```sh
/Users/wangzhen/code/ai/claude-light/bin/claude-light-state <state>
```

如果你把项目克隆到了其他目录，需要把 Claude Code 设置里的绝对路径改成你本机实际路径。

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
