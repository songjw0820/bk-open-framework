# BK7259 锁定 SHA 目标探针证据

## 环境

- Docker 镜像：`bekencorp/armino-idk:1.5`
- Image digest：`sha256:5c85a6428490220c796a77a2d72bd7371be409b55e0f9a1e844a7b7a8066c1ed`
- SDK commit：`584b947920c414230f9909375f008e7d6a2c6c6b`
- Solution commit：`a1e865e156e8dc31dfb360219c33a6a6d908ea98`
- 工具链：Arm GNU Toolchain 14.3.Rel1，`gcc/g++ 14.3.1`，`nm 2.44.0.20250616`

## 执行命令

```bash
./tools/spikes/bk7259_component_probe/probe.sh \
  --profile minimal --sdk-dir /work/sdk --build --clean-build

./tools/spikes/bk7259_component_probe/probe.sh \
  --profile robot --sdk-dir /work/sdk --solution-dir /work/solution \
  --build --clean-build
```

## 结果

| Profile | 产物 | 大小 | SHA256 | 结果 |
|---|---|---:|---|---|
| `minimal` | `build/bk7259/app/package/all-app.bin` | 2,875,168 B | `0c5fb5f9366fad35d9ae4e70da4b97f5e280ce993e5403d41e71b78e2298212a` | PASS |
| `minimal` | `build/bk7259/app/package/app_pack.rbl` | 1,426,080 B | `d8c244a5f7a887959fc8dd8c0286be10440e83bc48543503e41d362c28808cd0` | PASS |
| `robot` | `build/bk7259/beken_robot/package/all-app.bin` | 5,060,928 B | `4e009703d544cf229a7704ed13745921255d1db4f23db648684043b31dd9ba24` | PASS |
| `robot` | `build/bk7259/beken_robot/package/app_pack.rbl` | 2,963,312 B | `9abf7cec433c6c007c2cf6eb022858d647f02374ddddb3e4eb505c370767d704` | PASS |

两次探针均验证了 AP/CP 编译、Kconfig on/off 符号裁剪和 `all-app.bin` 打包。构建日志中的链接器报告包含既有的 `missing --end-group`、RWX LOAD segment 和 newlib `_gettimeofday` 警告，未阻止探针通过，需在产品发布前单独评估。

该证据只覆盖 Docker 目标构建，不代表 BK7259 实机启动、外设 DMA、WSS、音频或 OTA 已验收。

`robot` 的 `app_pack.rbl` 为 2,963,312 B，超过候选分区表中 1,960 KiB 的 `ota` 容量。该候选分区表未通过尺寸门禁，`bk_ota` 继续保持 deferred。
