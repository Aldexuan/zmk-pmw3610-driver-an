# 🔧 硬件功耗异常诊断方案

## 问题描述
- **空闲功耗**：700-900μA → 1.5mA（升高 600-800μA）
- **深睡功耗**：300μA → 700-800μA（升高 400-500μA）
- **所有驱动版本都一样** → 硬件问题

---

## 📋 诊断步骤（按优先级排序）

### ✅ 步骤 1: 快速排查传感器通信

#### 测试目的
验证 PMW3610 传感器是否损坏

#### 测试方法
添加以下测试代码到 `pmw3610_init()` 函数：

```c
// 在 pmw3610_init() 最后添加
LOG_INF("=== PMW3610 Hardware Test ===");

// 1. 读取 Product ID
uint8_t product_id = 0;
err = reg_read(dev, PMW3610_REG_PRODUCT_ID, &product_id);
if (err) {
    LOG_ERR("❌ Cannot read Product ID - sensor may be damaged!");
} else {
    LOG_INF("Product ID: 0x%02x (expected: 0x3E)", product_id);
    if (product_id != 0x3E) {
        LOG_ERR("❌ Wrong Product ID - sensor damaged or communication error!");
    }
}

// 2. 读取 PERFORMANCE 寄存器
uint8_t perf = 0;
err = reg_read(dev, PMW3610_REG_PERFORMANCE, &perf);
if (err) {
    LOG_ERR("❌ Cannot read PERFORMANCE register");
} else {
    LOG_INF("PERFORMANCE: 0x%02x (expected: 0x0D or 0x0F)", perf);
}

// 3. 读取 OBSERVATION 寄存器（自检）
uint8_t obs = 0;
err = reg_read(dev, PMW3610_REG_OBSERVATION, &obs);
if (err) {
    LOG_ERR("❌ Cannot read OBSERVATION register");
} else {
    LOG_INF("OBSERVATION: 0x%02x (expected: 0x0F after self-test)", obs);
}

LOG_INF("=== Hardware Test Complete ===");
```

#### 预期结果
- ✅ Product ID = 0x3E
- ✅ PERFORMANCE = 0x0D 或 0x0F
- ✅ OBSERVATION = 0x0F（自检通过）

#### 如果失败
- ❌ **无法读取** → SPI 通信失败，检查连接
- ❌ **Product ID 错误** → 传感器损坏或地址错误
- ❌ **能读取但值异常** → 传感器内部故障

---

### ✅ 步骤 2: 测量传感器单独功耗

#### 测试目的
确认是传感器本身功耗升高，还是其他部分

#### 测试方法 A: 软件隔离

在深睡眠状态下，尝试完全断开传感器电源：

```c
// 在 pmw3610_on_enter_sleep() 中添加
static int pmw3610_on_enter_sleep(const struct device *dev) {
    // ... 原有代码 ...
    
    // 测试：尝试写入 SHUTDOWN 命令（如果传感器支持）
    LOG_INF("TEST: Sending SHUTDOWN command to sensor");
    err = reg_write(dev, PMW3610_REG_SHUTDOWN, 0xB6);
    if (err) {
        LOG_WRN("Cannot send SHUTDOWN (sensor may not support)");
    }
    
    // 等待一下，测量功耗
    k_sleep(K_MSEC(100));
    
    // ... GPIO 释放代码 ...
}
```

**测量功耗变化**：
- 如果发送 SHUTDOWN 后功耗下降 500μA → 传感器本身有问题
- 如果功耗不变 → 传感器电源回路或其他部分有问题

#### 测试方法 B: 硬件隔离（推荐）

1. **断开传感器 VDD 引脚**（物理断开或通过开关）
2. **测量键盘功耗**
3. **对比差异**

**如果断开后功耗正常** → 传感器或其电源回路有问题

---

### ✅ 步骤 3: GPIO 引脚逐个测试

#### 测试目的
找出是否有 GPIO 引脚短路或漏电

#### 测试代码

```c
// 添加到 pmw3610_on_enter_sleep() 函数
static int test_gpio_power(const struct device *dev) {
    const struct pixart_config *config = dev->config;
    
    LOG_INF("=== GPIO Power Test ===");
    
    // 基准功耗（所有 GPIO 连接）
    LOG_INF("Baseline: all GPIO connected");
    k_sleep(K_SECONDS(2));
    LOG_INF("Measure current NOW (baseline)");
    k_sleep(K_SECONDS(5));
    
    // 测试 1: 释放 CS
    gpio_pin_configure_dt(&config->cs_gpio, GPIO_DISCONNECTED);
    LOG_INF("CS released");
    k_sleep(K_SECONDS(5));
    LOG_INF("Measure current NOW (CS disconnected)");
    
    // 测试 2: 释放 IRQ
    gpio_pin_configure_dt(&config->irq_gpio, GPIO_DISCONNECTED);
    LOG_INF("IRQ released");
    k_sleep(K_SECONDS(5));
    LOG_INF("Measure current NOW (CS+IRQ disconnected)");
    
    // 测试 3: 释放 SPI 数据线
    const struct device *gpio0 = DEVICE_DT_GET(DT_NODELABEL(gpio0));
    const struct device *gpio1 = DEVICE_DT_GET(DT_NODELABEL(gpio1));
    
    gpio_pin_configure(gpio0, 10, GPIO_DISCONNECTED);  // MOSI/MISO
    gpio_pin_configure(gpio1, 13, GPIO_DISCONNECTED);  // SCK
    LOG_INF("SPI pins released");
    k_sleep(K_SECONDS(5));
    LOG_INF("Measure current NOW (all GPIO disconnected)");
    
    LOG_INF("=== GPIO Power Test Complete ===");
    return 0;
}
```

#### 分析结果
- 释放某个引脚后功耗显著下降 → 该引脚有问题
- 释放所有引脚后功耗仍高 → 不是 GPIO 问题

---

### ✅ 步骤 4: 检查传感器电源

#### 硬件测量

1. **测量传感器 VDD 引脚电压**
   - 用万用表测量
   - 应该是 **3.3V ± 0.1V**
   - 如果电压异常 → 电源稳压器问题

2. **测量传感器 VDD 引脚电流**
   - 串联电流表
   - 正常应该是：
     - RUN 模式：~2mA
     - REST1：~500μA
     - REST2：~200μA
     - REST3：~100μA
   - 如果一直 >2mA → 传感器没有降频

3. **检查电源去耦电容**
   - 传感器 VDD 引脚附近应该有 0.1μF 和 4.7μF 电容
   - 用万用表电容档测量容值
   - 如果容值异常 → 电容失效

---

### ✅ 步骤 5: 检查其他外设

#### 逐个禁用外设

在键盘配置文件中逐个禁用外设：

```conf
# 禁用显示屏
# CONFIG_DISPLAY=n

# 禁用 RGB LED
# CONFIG_ZMK_RGB_UNDERGLOW=n

# 禁用其他传感器
# CONFIG_SENSOR=n

# 只保留 PMW3610
CONFIG_PMW3610=y
```

每次修改后重新编译、烧录、测量功耗。

#### 分析
- 禁用某个外设后功耗恢复正常 → 该外设有问题
- 禁用所有外设功耗仍高 → 基础系统或 MCU 问题

---

### ✅ 步骤 6: 对比测试

#### 使用另一块相同的键盘

如果有另一块相同的键盘：
1. 烧录**完全相同的固件**
2. 测量功耗
3. 对比结果

**如果另一块正常** → 这块键盘硬件损坏  
**如果另一块也异常** → 固件或配置问题

---

## 🎯 最可能的原因排序

基于症状分析：

### 🥇 原因 1: PMW3610 传感器损坏（概率 60%）

**特征**：
- ✅ 功耗升高 500μA
- ✅ 空闲和深睡都升高相似幅度
- ✅ 所有驱动版本都一样

**验证方法**：
1. 步骤 1：读取寄存器
2. 步骤 2：测量传感器单独功耗

**如果确认**：
- 更换传感器
- 或尝试其他型号传感器（如 PAW3395）

---

### 🥈 原因 2: 传感器电源回路异常（概率 25%）

**特征**：
- ✅ 传感器能通信，但功耗高
- ✅ VDD 电压可能异常

**验证方法**：
1. 步骤 4：测量电源电压和电流
2. 检查电源稳压器和滤波电容

**如果确认**：
- 更换电源稳压器
- 更换去耦电容
- 检查 PCB 走线

---

### 🥉 原因 3: GPIO 引脚短路（概率 10%）

**特征**：
- ✅ 功耗升高
- ✅ 释放 GPIO 后功耗下降

**验证方法**：
1. 步骤 3：GPIO 引脚逐个测试

**如果确认**：
- 检查焊接
- 检查 PCB 是否有短路
- 重新焊接

---

### 🏅 原因 4: 其他外设故障（概率 5%）

**特征**：
- ✅ 禁用某个外设后功耗恢复

**验证方法**：
1. 步骤 5：逐个禁用外设

---

## 📊 诊断流程图

```
开始
  ↓
步骤1: 能读取传感器寄存器？
  ├─ 否 → SPI 通信故障，检查连接
  ↓ 是
步骤1: Product ID 正确？
  ├─ 否 → 传感器损坏，更换
  ↓ 是
步骤2: 断开传感器后功耗正常？
  ├─ 是 → 传感器或其电源回路故障
  ↓ 否
步骤3: 释放 GPIO 后功耗下降？
  ├─ 是 → GPIO 引脚短路/漏电
  ↓ 否
步骤5: 禁用外设后功耗恢复？
  ├─ 是 → 该外设故障
  ↓ 否
步骤4: 检查电源电压
  └─ 异常 → 电源系统故障
```

---

## 🔧 临时解决方案

如果传感器确实损坏，可以考虑：

### 方案 1: 更换传感器
- 购买新的 PMW3610
- 重新焊接

### 方案 2: 使用其他传感器
- PAW3395（更高端）
- PMW3389（备选）
- 需要修改驱动代码

### 方案 3: 禁用传感器
- 如果暂时不用鼠标功能
- 完全禁用 PMW3610
- 降低功耗

---

## 📝 测试记录模板

| 测试项 | 结果 | 功耗 | 结论 |
|--------|------|------|------|
| Product ID | 0x3E / 错误 / 无法读取 | - | 传感器状态 |
| PERFORMANCE | 0x0D / 其他 | - | 寄存器状态 |
| 断开传感器电源 | - | XXX μA | 传感器贡献 |
| 释放 CS | - | XXX μA | CS 引脚状态 |
| 释放 IRQ | - | XXX μA | IRQ 引脚状态 |
| 释放 SPI | - | XXX μA | SPI 引脚状态 |
| VDD 电压 | X.XX V | - | 电源状态 |
| VDD 电流 | X.XX mA | - | 传感器功耗 |

---

## 🎯 下一步

1. **先做步骤 1**：确认传感器是否能正常通信
2. **再做步骤 2**：确认是否是传感器本身功耗升高
3. **根据结果决定下一步**

如果确认是传感器损坏，可能需要：
- 🔧 更换传感器
- 🔬 深入硬件调试
- 📞 联系传感器供应商
