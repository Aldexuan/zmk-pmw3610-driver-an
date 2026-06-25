# ✅ FORCE_AWAKE 功能移除完成

## 📋 修改内容总结

### 1️⃣ Kconfig 配置文件
**文件**: `Kconfig`

**修改**: 删除 `CONFIG_PMW3610_FORCE_AWAKE` 配置选项

```diff
-config PMW3610_FORCE_AWAKE
-    bool "PMW3610 forced awake mode"
-    help
-      This setting forces the sensor to always be in the RUN state.
-
 config PMW3610_RUN_DOWNSHIFT_TIME_MS
```

**效果**: 用户无法通过 Kconfig 启用 FORCE_AWAKE

---

### 2️⃣ 头文件定义
**文件**: `src/pmw3610.h`

**修改**: 移除条件编译，强制设置为 0x00

```diff
-#ifdef CONFIG_PMW3610_FORCE_AWAKE
-#define PMW3610_FORCE_MODE_VALUE 0xF0
-#else
+// FORCE_AWAKE 功能已完全移除，防止意外启用导致功耗升高
+// 传感器将始终允许自动降频进入 REST 模式以节省电力
+// 如果需要高性能模式，应该通过调整 downshift 时间来实现，而不是禁用降频
 #define PMW3610_FORCE_MODE_VALUE 0x00
-#endif
```

**效果**: `PMW3610_PERFORMANCE_VALUE` 永远不会包含 FORCE_AWAKE 位

---

### 3️⃣ 驱动初始化代码
**文件**: `src/pmw3610.c`

**修改**: 在 `pmw3610_async_init_configure()` 中添加强制清除和验证逻辑

```c
// 强制清除 FORCE_AWAKE 位，防止传感器无法降频导致功耗升高
if (!err) {
    // 确保 FORCE_AWAKE (bit 4) 被清除
    uint8_t perf_value = PMW3610_PERFORMANCE_VALUE & ~0x10;
    
    err = reg_write(dev, PMW3610_REG_PERFORMANCE, perf_value);
    LOG_INF("Set PERFORMANCE register: 0x%02x", perf_value);
    
    // 验证写入是否成功
    k_msleep(10);
    uint8_t readback = 0;
    int read_err = reg_read(dev, PMW3610_REG_PERFORMANCE, &readback);
    
    if (!read_err) {
        LOG_INF("PERFORMANCE readback: 0x%02x", readback);
        
        // 检查 FORCE_AWAKE 位
        if (readback & 0x10) {
            LOG_ERR("⚠️  FORCE_AWAKE bit is stuck! Forcing clear...");
            
            // 多次尝试清除（最多 5 次）
            for (int retry = 0; retry < 5; retry++) {
                err = reg_write(dev, PMW3610_REG_PERFORMANCE, perf_value);
                k_msleep(20);
                reg_read(dev, PMW3610_REG_PERFORMANCE, &readback);
                
                LOG_INF("Clear retry %d: readback = 0x%02x", retry + 1, readback);
                
                if (!(readback & 0x10)) {
                    LOG_INF("✅ FORCE_AWAKE cleared successfully on retry %d", retry + 1);
                    break;
                }
            }
            
            // 最终检查
            if (readback & 0x10) {
                LOG_ERR("❌ Failed to clear FORCE_AWAKE after 5 retries!");
                LOG_ERR("   Sensor will stay in RUN mode, power consumption ~500μA higher");
                LOG_ERR("   Consider hardware reset or sensor replacement");
            } else {
                LOG_INF("✅ FORCE_AWAKE successfully disabled");
            }
        } else {
            LOG_INF("✅ FORCE_AWAKE is disabled (bit 4 = 0)");
            LOG_INF("   Sensor can enter REST modes for power saving");
        }
        
        // 输出完整的寄存器解析
        LOG_INF("PERFORMANCE register breakdown:");
        LOG_INF("  FORCE_AWAKE (bit 4): %s", (readback & 0x10) ? "1 (ENABLED ⚠️)" : "0 (disabled ✅)");
        LOG_INF("  Polling rate: %s", 
                (readback & 0x0C) == 0x0C ? "250Hz" : 
                (readback & 0x0C) == 0x00 ? "125Hz" : "other");
    } else {
        LOG_WRN("Cannot read back PERFORMANCE register for verification");
    }
}
```

**效果**: 
- 每次初始化都会强制清除 FORCE_AWAKE 位
- 验证清除是否成功
- 如果被"卡住"，最多重试 5 次
- 输出详细的诊断日志

---

## 🎯 修改目的

### 1. 防止意外启用
- 删除 Kconfig 选项，用户无法通过配置启用
- 头文件中移除条件编译，永远使用 0x00

### 2. 清除持久化配置
- 每次初始化都强制写入 0x00（不包含 FORCE_AWAKE）
- 即使传感器之前被设置为 FORCE_AWAKE，也会被清除

### 3. 验证和诊断
- 读回寄存器值，确认写入成功
- 如果发现 FORCE_AWAKE 位仍然是 1，尝试多次清除
- 输出详细的日志，方便诊断

---

## 📊 预期效果

### 编译时
```
PMW3610_FORCE_MODE_VALUE = 0x00 (固定值)
PMW3610_PERFORMANCE_VALUE = 0x0D (250Hz) 或 0x00 (125Hz)
```

### 运行时日志（正常情况）
```
Set PERFORMANCE register: 0x0d
PERFORMANCE readback: 0x0d
✅ FORCE_AWAKE is disabled (bit 4 = 0)
   Sensor can enter REST modes for power saving
PERFORMANCE register breakdown:
  FORCE_AWAKE (bit 4): 0 (disabled ✅)
  Polling rate: 250Hz
```

### 运行时日志（如果之前被设置）
```
Set PERFORMANCE register: 0x0d
PERFORMANCE readback: 0x1d
⚠️  FORCE_AWAKE bit is stuck! Forcing clear...
Clear retry 1: readback = 0x0d
✅ FORCE_AWAKE cleared successfully on retry 1
PERFORMANCE register breakdown:
  FORCE_AWAKE (bit 4): 0 (disabled ✅)
  Polling rate: 250Hz
```

### 功耗测量
- **空闲功耗**: 从 1.5mA → **700-900μA** ✅
- **深睡功耗**: 从 700-800μA → **300μA** ✅

---

## 🔧 测试步骤

### 1. 编译并烧录
```bash
# 清理并重新编译
west build -b <your_board> -p
west flash
```

### 2. 查看日志
连接串口或 USB 日志，应该看到：
```
✅ FORCE_AWAKE is disabled (bit 4 = 0)
```

如果看到重试信息，说明成功清除了之前的持久化配置。

### 3. 测量功耗
- 等待 30 秒进入 IDLE 状态
- 测量电流应该降至 700-900μA
- 等待 15 分钟进入 SLEEP 状态
- 测量电流应该降至 300μA

---

## ⚠️ 故障排查

### 如果日志显示清除失败
```
❌ Failed to clear FORCE_AWAKE after 5 retries!
```

**可能原因**：
1. 传感器硬件故障
2. SPI 通信问题
3. 传感器固件异常

**解决方法**：
1. 尝试完全断电重启（不是软复位）
2. 检查 SPI 连接
3. 更换传感器

### 如果功耗仍然很高

**检查项**：
1. 确认日志显示 `FORCE_AWAKE is disabled`
2. 检查是否真的进入了 IDLE/SLEEP 状态（查看 ZMK 状态日志）
3. 检查其他外设功耗（显示屏、LED 等）
4. 使用 `HARDWARE-DEBUG.md` 中的诊断方法

---

## 📝 技术细节

### PERFORMANCE 寄存器（0x11）位定义
```
Bit 7-5: 保留
Bit 4:   FORCE_AWAKE
         0 = 允许降频 (正常)
         1 = 强制 RUN 模式 (高功耗)
Bit 3-2: 采样率配置
Bit 1-0: 轮询率
         00 = 125Hz
         01 = 250Hz
```

### 正常值
- 250Hz 轮询率: `0x0D` (二进制: 0000_1101)
  - Bit 4 (FORCE_AWAKE) = 0 ✅
  - Bits 3-2 = 11
  - Bits 1-0 = 01

- 125Hz 轮询率: `0x00` (二进制: 0000_0000)
  - Bit 4 (FORCE_AWAKE) = 0 ✅
  - 所有其他位 = 0

### 异常值（已修复）
- 250Hz + FORCE_AWAKE: `0x1D` (二进制: 0001_1101)
  - Bit 4 (FORCE_AWAKE) = 1 ❌
  - 会被强制清除

---

## ✅ 修改完成确认

- [x] 移除 Kconfig 中的 `CONFIG_PMW3610_FORCE_AWAKE` 选项
- [x] 修改头文件，固定 `PMW3610_FORCE_MODE_VALUE = 0x00`
- [x] 添加初始化时的强制清除逻辑
- [x] 添加读回验证和重试机制
- [x] 添加详细的诊断日志
- [x] 创建说明文档

**下一步**: 编译、烧录、测试功耗
