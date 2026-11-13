# Task 1 report

## Fix round 1

本轮修复了审核指出的四类问题。

- `LICENSE` 已替换为完整 Apache License 2.0 正文。
- `README.md` 和 `README_CN.md` 已明确第一阶段只支持 BK7259 的 `bk7259_robot`，并限制 profile 为 `minimal` 和 `robot`。
- `tools/manifest.py` 已增加 profile、canonical SDK、minimal solution 和 robot solution 语义校验。SDK 固定为 `https://github.com/bekencorp/bk_avdk_smp.git` 的 `release/v4.0.1.11` 和锁定 SHA。robot solution 固定为 `https://github.com/bekencorp/bk_solution_ai.git` 的同一 release ref 和锁定 SHA。
- `tools/tests/test_manifest.py` 已补充 canonical repository/ref/SHA、minimal solution、robot solution、profile 失败用例，同时保留缺字段、ref 不一致、芯片和板卡失败用例。

测试文件为 `tools/tests/test_manifest.py`。

实际运行命令及输出如下。

```text
$ python3 -m pytest tools/tests/test_manifest.py -v
/opt/homebrew/opt/python@3.14/bin/python3.14: No module named pytest

$ python3 tools/manifest.py platforms/bk7259_minimal.yaml platforms/bk7259_robot.yaml
valid: platforms/bk7259_minimal.yaml
valid: platforms/bk7259_robot.yaml

$ python3 -m compileall -q tools

$ python3 <stdlib smoke script>
manifest smoke tests: 12 passed
```

剩余限制是当前环境未安装 pytest，因此正式 pytest 输出未能取得。两个现有 profile 的 CLI 校验、12 个有效和无效 smoke 场景及标准库编译检查均已通过。

## Fix round 2

补充了 robot solution 三个 canonical 字段的直接失败测试。

- `solution.repository` 改为非 canonical 地址时必须失败。
- `solution.ref` 改为非 canonical release ref 时必须命中 canonical solution 规则。
- `solution.sha` 改为全零 SHA 时必须失败。

实际运行命令及输出如下。

```text
$ python3 <fix round 2 stdlib smoke script>
fix round 2 smoke tests: 5 passed

$ python3 tools/manifest.py platforms/bk7259_minimal.yaml platforms/bk7259_robot.yaml
valid: platforms/bk7259_minimal.yaml
valid: platforms/bk7259_robot.yaml

$ python3 -m compileall -q tools

$ python3 -m pytest tools/tests/test_manifest.py -v
/opt/homebrew/opt/python@3.14/bin/python3.14: No module named pytest
```
