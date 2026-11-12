# Task 0 报告：BK7259 AP/CP 构建集成与硬件事实清单

## 交付物

- `platforms/bk7259_minimal.yaml`：仅依赖 Armino SMP SDK；`solution: null`。
- `platforms/bk7259_robot.yaml`：声明匹配的 `bk_solution_ai` ref；SDK/solution SHA 保留待验证占位。
- `docs/platforms/bk7259_v4.0.1.11.md`：记录 BK7259、AP/CP、GPIO10/11 CP UART0 和待验证项。
- `docs/legal/bk_solution_ai-dependency-inventory.md`：记录公共框架与 robot profile 的依赖边界和审计入口。
- `tools/spikes/bk7259_component_probe/README.md`：记录 AP/CP 注入入口、通过条件和未执行状态。

## 证据与限制

静态事实来自 `/Users/soong/Documents/work/ai_work/armino/bk7259/robot_bk7259/bk_solution_ai`。本地 solution HEAD 为 `d79bcd5b3b9d0aec57da679e62777c8e31eceb7f`，但没有可用的匹配 SDK upstream checkout，因此没有把它冒充为 `release/v4.0.1.11` 锁定 SHA。

AP/CP 空组件编译、镜像打包和最终 `all-app.bin` 验证均未运行，原因是工具链和锁定版本 SDK checkout 不可用。

## 验证命令

- `git diff --check`：用于检查本次文档/YAML 改动的空白错误。
- `git status --short`、`git diff --stat`：用于确认交付物范围。

## 提交

本报告与交付物随同一提交提交；提交后应在父任务中记录 commit SHA。
