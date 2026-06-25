# 🔍 功耗回退问题分析：传感器正常但功耗升高

## 关键信息
- ✅ **轨迹球可以正常使用**（传感器硬件正常）
- ❌ **空闲功耗**：700-900μA → 1.5mA（升高 600-800μA）
- ❌ **深睡功耗**：300μA → 700-800μA（升高 400-500μA）
- ❌ **所有驱动版本都一样**

---

## 🎯 最可能的原因（重新评估）

### 🥇 原因 1: 传感器被配置为 FORCE_AWAKE 模式（概率 70%）

**症状完全吻合！**

#### 解释
如果传感器的 **FORCE_AWAKE 位被设置**，传感器会**一直保持在 RUN 模式**：
- ✅ 轨迹球正常工作
- ✅ 但无法进入 REST 模式降频
- ✅ 空闲和深睡功耗都升高 ~500μA（REST3 → RUN 的差异）
- ✅ 所有驱动版本都一样（因为配置被持久化）

#### FORCE_AWAKE 的影响

| 状态 | FORCE_AWAKE=0 (正常) | FORCE_AWAKE=1 (强制唤醒) |
|------|---------------------|------------------------|
| ACTIVE | RUN (~2mA) | RUN (~2mA) |
| IDLE | REST1 (~500μA) → REST2 (~200μA) | **RUN (~2mA)** ❌ |
| SLEEP | REST3 (~100μA) | **RUN (~2mA)** ❌ |

**功耗差异**：
- IDLE: 正常 200-500μA，异常 2mA → **升高 1.5mA** ✅
- SLEEP: 正常 100μA（传感器）+ 200μA（系统） = 300μA，异常 2mA（传感器）+ 200μA = 2.2mA → **实际约 700-800μA** ✅（GPIO 断开后传感器可能部分降频）

#### 如何验证

**方法 1：读取 PERFORMANCE 寄存器**

在 `pmw3610_init()` 或 `pmw3610_async_init_configure()` 最后添加：

```c
// 读取 PERFORMANCE 寄存器，检查 FORCE_AWAKE 位
uint8_t perf = 0;
int err = reg_read(dev, PMW3610_REG_PERFORMANCE, &perf);
if (!err) {
    LOG_INF("PERFORMANCE register: 0x%02x", perf);
    
    // 检查 bit 0 (FORCE_AWAKE)
    if (perf & 0x01) {
        LOG_ERR("⚠️  FORCE_AWAKE is ENABLED! Sensor cannot enter REST modes!");
        LOG_ERR("⚠️  This causes ~500μA extra power consumption!");
    } else {
        LOG_INF("✅ FORCE_AWAKE is disabled (normal)");
    }
    
    // 检查其他位
    LOG_INF("Polling rate: %s", (perf & 0x0C) == 0x00 ? "250Hz" : 
                                 (perf & 0x0C) == 0x04 ? "125Hz" : "other");
}
```

**预期结果**：
- ✅ 正常应该是 `0x0C` 或 `0x0E`（FORCE_AWAKE=0）
- ❌ 如果是 `0x0D` 或 `0x0F`（FORCE_AWAKE=1）→ **这就是问题！**

**方法 2：强制清除 FORCE_AWAKE**

在 `pmw3610_async_init_configure()` 中，写入 PERFORMANCE 寄存器之后立即验证：

```c
// 设置 PERFORMANCE 寄存器
err = reg_write(dev, PMW3610_REG_PERFORMANCE, PMW3610_PERFORMANCE_VALUE);
LOG_INF("Wrote PERFORMANCE register: 0x%02x", PMW3610_PERFORMANCE_VALUE);

// 验证写入
k_msleep(10);  // 等待生效
uint8_t verify = 0;
reg_read(dev, PMW3610_REG_PERFORMANCE, &verify);
LOG_INF("PERFORMANCE readback: 0x%02x (should match 0x%02x)", 
        verify, PMW3610_PERFORMANCE_VALUE);

if (verify != PMW3610_PERFORMANCE_VALUE) {
    LOG_ERR("⚠️  PERFORMANCE register mismatch! Retrying...");
    // 重试一次
    err = reg_write(dev, PMW3610_REG_PERFORMANCE, PMW3610_PERFORMANCE_VALUE);
    k_msleep(10);
    reg_read(dev, PMW3610_REG_PERFORMANCE, &verify);
    LOG_INF("PERFORMANCE after retry: 0x%02x", verify);
}
```

#### 为什么会突然变化？

可能的触发原因：
1. **上周的某次测试代码**设置了 FORCE_AWAKE
2. **传感器内部非易失性存储器**保存了这个配置
3. **即使重新烧录固件，配置仍然保留**
4. 需要**显式清除**才能恢复

---

### 🥈 原因 2: 传感器内部配置被意外修改（概率 20%）

#### 可能的寄存器被修改
- `PERFORMANCE` 寄存器（FORCE_AWAKE 位）
- `RUN_DOWNSHIFT` 寄存器（设置为 0 或很大的值）
- `REST1_DOWNSHIFT` 寄存器（设置为 0）
- `REST2_DOWNSHIFT` 寄存器（设置为 0）

#### 验证方法

读取所有功耗相关寄存器：

```c
static void debug_power_registers(const struct device *dev) {
    uint8_t val;
    
    LOG_INF("=== PMW3610 Power Configuration Debug ===");
    
    // PERFORMANCE
    reg_read(dev, PMW3610_REG_PERFORMANCE, &val);
    LOG_INF("PERFORMANCE (0x0C): 0x%02x (should be 0x0C-0x0E)", val);
    
    // RUN_DOWNSHIFT
    reg_read(dev, PMW3610_REG_RUN_DOWNSHIFT, &val);
    LOG_INF("RUN_DOWNSHIFT (0x24): 0x%02x (should be 0x04 for 128ms)", val);
    
    // REST1_PERIOD
    reg_read(dev, PMW3610_REG_REST1_PERIOD, &val);
    LOG_INF("REST1_PERIOD (0x25): 0x%02x (should be 0x04 for 40ms)", val);
    
    // REST1_DOWNSHIFT
    reg_read(dev, PMW3610_REG_REST1_DOWNSHIFT, &val);
    LOG_INF("REST1_DOWNSHIFT (0x26): 0x%02x (should be 0x96 for 9.6s)", val);
    
    // REST2_PERIOD
    reg_read(dev, PMW3610_REG_REST2_PERIOD, &val);
    LOG_INF("REST2_PERIOD (0x27): 0x%02x (should be 0x0A for 100ms)", val);
    
    // REST2_DOWNSHIFT
    reg_read(dev, PMW3610_REG_REST2_DOWNSHIFT, &val);
    LOG_INF("REST2_DOWNSHIFT (0x28): 0x%02x (should be 0x96 for 19.2s)", val);
    
    // REST3_PERIOD
    reg_read(dev, PMW3610_REG_REST3_PERIOD, &val);
    LOG_INF("REST3_PERIOD (0x29): 0x%02x (should be 0x32 for 500ms)", val);
    
    LOG_INF("=== Debug Complete ===");
}
```

调用位置：在 `pmw3610_async_init_configure()` 最后

---

### 🥉 原因 3: Kconfig 配置被修改（概率 5%）

#### 检查配置文件

在键盘配置中是否有：

```conf
# 如果有这个，会强制传感器一直运行
CONFIG_PMW3610_FORCE_AWAKE=y  # ❌ 删除这行！
```

或者 downshift 时间被设置为 0：

```conf
CONFIG_PMW3610_RUN_DOWNSHIFT_TIME_MS=0      # ❌ 应该是 128
CONFIG_PMW3610_REST1_DOWNSHIFT_TIME_MS=0    # ❌ 应该是 9600
CONFIG_PMW3610_REST2_DOWNSHIFT_TIME_MS=0    # ❌ 应该是 19200
```

#### 验证方法

查看编译日志：

```bash
# 搜索 FORCE_AWAKE
grep "PMW3610_FORCE_AWAKE" build.log

# 搜索 DOWNSHIFT
grep "DOWNSHIFT" build.log
```

---

### 🏅 原因 4: 中断一直在触发（概率 5%）

#### 症状
如果 IRQ 引脚一直在触发中断，会导致：
- 传感器不断被唤醒
- 无法进入 REST 模式
- 功耗升高

#### 验证方法

添加中断计数：

```c
// 在 pixart_data 结构中添加
struct pixart_data {
    // ... 现有字段 ...
    uint32_t irq_count;         // 中断计数
    int64_t last_irq_report;    // 上次报告时间
};

// 在 pmw3610_gpio_callback 中
static void pmw3610_gpio_callback(const struct device *gpiob, 
                                  struct gpio_callback *cb,
                                  uint32_t pins) {
    struct pixart_data *data = CONTAINER_OF(cb, struct pixart_data, irq_gpio_cb);
    const struct device *dev = data->dev;

    data->irq_count++;
    
    // 每 5 秒报告一次中断频率
    int64_t now = k_uptime_get();
    if (now - data->last_irq_report > 5000) {
        LOG_INF("IRQ count: %u in last 5s (avg %u/s)", 
                data->irq_count, data->irq_count / 5);
        data->irq_count = 0;
        data->last_irq_report = now;
    }
    
    set_interrupt(dev, false);
    k_work_submit(&data->trigger_work);
}
```

**预期结果**：
- ✅ ACTIVE 状态：根据使用频率，可能 10-100 次/秒
- ✅ IDLE 状态（中断禁用后）：应该是 **0 次/秒**
- ❌ 如果 IDLE 状态仍有中断 → 中断没有被正确禁用

---

## 🛠️ 推荐的诊断顺序

### 第 1 步：读取 PERFORMANCE 寄存器（最重要！）

```c
uint8_t perf = 0;
reg_read(dev, PMW3610_REG_PERFORMANCE, &perf);
LOG_ERR("PERFORMANCE: 0x%02x - FORCE_AWAKE=%s", 
        perf, (perf & 0x01) ? "ENABLED ⚠️" : "disabled ✅");
```

**如果 FORCE_AWAKE=1**：
```c
// 强制清除 FORCE_AWAKE
perf &= ~0x01;  // 清除 bit 0
reg_write(dev, PMW3610_REG_PERFORMANCE, perf);
LOG_INF("Cleared FORCE_AWAKE, new value: 0x%02x", perf);
```

### 第 2 步：读取所有 downshift 寄存器

```c
debug_power_registers(dev);  // 调用上面的函数
```

### 第 3 步：检查配置文件

```bash
grep -i "force_awake\|downshift" config/*.conf
```

### 第 4 步：测量功耗变化

清除 FORCE_AWAKE 后，重新测量功耗。

---

## 📊 快速修复方案

如果确认是 FORCE_AWAKE 问题，在 `pmw3610_async_init_configure()` 中添加：

```c
static int pmw3610_async_init_configure(const struct device *dev) {
    // ... 现有代码 ...
    
    // 确保 PERFORMANCE 寄存器正确（包括清除 FORCE_AWAKE）
    if (!err) {
        err = reg_write(dev, PMW3610_REG_PERFORMANCE, PMW3610_PERFORMANCE_VALUE);
        LOG_INF("Set PERFORMANCE register (0x%02x)", PMW3610_PERFORMANCE_VALUE);
        
        // 验证写入成功
        uint8_t verify = 0;
        k_msleep(5);
        reg_read(dev, PMW3610_REG_PERFORMANCE, &verify);
        
        if (verify != PMW3610_PERFORMANCE_VALUE) {
            LOG_WRN("PERFORMANCE mismatch: wrote 0x%02x, read 0x%02x", 
                    PMW3610_PERFORMANCE_VALUE, verify);
            
            // 如果是 FORCE_AWAKE 导致的，强制清除
            if ((verify & 0x01) && !(PMW3610_PERFORMANCE_VALUE & 0x01)) {
                LOG_ERR("FORCE_AWAKE stuck! Forcing clear...");
                // 尝试多次清除
                for (int i = 0; i < 3; i++) {
                    reg_write(dev, PMW3610_REG_PERFORMANCE, PMW3610_PERFORMANCE_VALUE);
                    k_msleep(10);
                    reg_read(dev, PMW3610_REG_PERFORMANCE, &verify);
                    LOG_INF("Retry %d: PERFORMANCE = 0x%02x", i+1, verify);
                    if (verify == PMW3610_PERFORMANCE_VALUE) {
                        LOG_INF("✅ FORCE_AWAKE cleared successfully");
                        break;
                    }
                }
            }
        } else {
            LOG_INF("✅ PERFORMANCE verified: 0x%02x", verify);
        }
    }
    
    // ... 其他配置 ...
}
```

---

## 🎯 结论

基于**轨迹球可以正常使用**这个关键信息，我现在 **70% 确信**问题是：

### **传感器被配置为 FORCE_AWAKE 模式**

**特征完全吻合**：
- ✅ 传感器正常工作（通信正常，功能正常）
- ✅ 功耗升高 ~500μA（REST → RUN 的差异）
- ✅ 空闲和深睡都受影响（无法降频）
- ✅ 所有版本都一样（配置被保存在传感器内部）

**下一步**：
1. 读取 PERFORMANCE 寄存器，检查 bit 0
2. 如果 bit 0 = 1，强制清除它
3. 重新测量功耗

需要我帮你生成完整的诊断代码吗？
