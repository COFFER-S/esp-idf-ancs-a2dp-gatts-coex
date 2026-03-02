# ESP-IDF ANCS + A2DP + GATTS 编译错误修复报告

## 修复时间
2026-03-01

## 修复内容

### 1. `a2dp_sink.c` 第 298-299 行 - 结构体成员名错误
**问题**：使用了错误的结构体成员名 `media_ctrl_ack`
**修复**：改为正确的成员名 `media_ctrl_stat`

```c
// 修复前
ESP_LOGI(TAG, "A2DP media control ack: cmd=%d, status=%d",
         param->media_ctrl_ack.cmd, param->media_ctrl_ack.status);

// 修复后
ESP_LOGI(TAG, "A2DP media control ack: cmd=%d, status=%d",
         param->media_ctrl_stat.cmd, param->media_ctrl_stat.status);
```

### 2. `a2dp_sink.c` 第 321 行 - 格式说明符错误
**问题**：`uint32_t` 类型使用了 `%u` 格式说明符
**修复**：改为 `%lu` 并添加 `(unsigned long)` 类型转换

```c
// 修复前
ESP_LOGI(TAG, "A2DP: %u packets, %llu bytes, %u pps",
         s_a2dp_ctx.packet_count, s_a2dp_ctx.byte_count,
         s_a2dp_ctx.packet_count / 5);

// 修复后
ESP_LOGI(TAG, "A2DP: %lu packets, %llu bytes, %lu pps",
         (unsigned long)s_a2dp_ctx.packet_count, s_a2dp_ctx.byte_count,
         (unsigned long)(s_a2dp_ctx.packet_count / 5));
```

### 3. `a2dp_sink.c` 第 367 行 - 格式说明符错误
**问题**：`uint32_t` 类型使用了 `%x` 格式说明符
**修复**：改为 `%lx` 并添加 `(unsigned long)` 类型转换

```c
// 修复前
ESP_LOGI(TAG, "AVRC TG remote features: 0x%x", param->rmt_feats.feat_mask);

// 修复后
ESP_LOGI(TAG, "AVRC TG remote features: 0x%lx", (unsigned long)param->rmt_feats.feat_mask);
```

## 构建结果

✅ **构建成功** - 所有编译错误已修复，项目成功构建

```
[1105/1105] Generating binary image from built executable
Merged 2 ELF sections
Successfully created esp32 image.
```

## 生成文件

- `build/ancs_a2dp_gatts_coex.bin` - 应用程序二进制文件
- `build/ancs_a2dp_gatts_coex.elf` - ELF 可执行文件
- `build/partition_table/partition-table.bin` - 分区表
- `build/bootloader/bootloader.bin` - 引导加载程序

## 下一步

1. ✅ 编译错误修复完成
2. 🔄 推送到 GitHub（等待用户确认目标仓库）
3. 🔄 QEMU 测试验证（构建成功后可运行）
