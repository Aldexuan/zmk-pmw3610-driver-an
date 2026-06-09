# PMW3610 轨迹球驱动完整使用手册

> 适用项目：`zmk-pmw3610-driver-an`
> 所有解释均为中文，代码块保持原始格式

---

## 目录

1. [硬件接入（overlay）](#1-硬件接入overlay)
2. [基础功能开关（conf）](#2-基础功能开关conf)
3. [CPI / 光标速度](#3-cpi--光标速度)
4. [狙击模式（Snipe）](#4-狙击模式snipe)
5. [滚动模式（Scroll）](#5-滚动模式scroll)
6. [自动鼠标层（Automouse）](#6-自动鼠标层automouse)
7. [球动作层（Ball Action）](#7-球动作层ball-action)
8. [传感器方向与轴翻转](#8-传感器方向与轴翻转)
9. [鼠标加速度](#9-鼠标加速度)
10. [低功耗设置](#10-低功耗设置)
11. [运行时动态调节键值（&pms）](#11-运行时动态调节键值pms)
12. [完整配置示例](#12-完整配置示例)

---

## 1. 硬件接入（overlay）

在 `.overlay` 文件中声明 SPI 总线和传感器节点。

```dts
/* SPI 引脚低功耗配置 */
&pinctrl {
    spi1_default: spi1_default {
        group1 {
            psels = <NRF_PSEL(SPIM_SCK,  1, 13)>,
                    <NRF_PSEL(SPIM_MOSI, 0, 10)>,
                    <NRF_PSEL(SPIM_MISO, 0, 10)>;
        };
    };
    spi1_sleep: spi1_sleep {
        group1 {
            psels = <NRF_PSEL(SPIM_SCK,  1, 13)>,
                    <NRF_PSEL(SPIM_MOSI, 0, 10)>,
                    <NRF_PSEL(SPIM_MISO, 0, 10)>;
            low-power-enable;   /* 休眠时引脚进入低功耗状态 */
        };
    };
};

&spi1 {
    status = "okay";
    compatible = "nordic,nrf-spim";
    pinctrl-0 = <&spi1_default>;
    pinctrl-1 = <&spi1_sleep>;
    pinctrl-names = "default", "sleep";
    cs-gpios = <&gpio0 9 GPIO_ACTIVE_LOW>;  /* 片选引脚 */

    trackball: trackball@0 {
        status = "okay";
        compatible = "zmk,pmw3610";
        reg = <0>;
        spi-max-frequency = <2000000>;      /* SPI 最大频率 2 MHz */
        irq-gpios = <&gpio1 11 (GPIO_ACTIVE_LOW | GPIO_PULL_UP)>;  /* 中断引脚 */

        /* 功能层绑定（层编号从 0 开始） */
        automouse-layer = <4>;   /* 触球自动激活的鼠标层 */
        scroll-layers = <5>;     /* 在此层拨动轨迹球触发滚动 */
        snipe-layers  = <6>;     /* 在此层拨动轨迹球触发狙击（低速精准） */
    };
};

/* 输入监听器：把传感器事件接入 ZMK 输入系统 */
/ {
    trackball_listener {
        compatible = "zmk,input-listener";
        device = <&trackball>;
    };
};
```

### DTS 节点属性一览

| 属性 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `irq-gpios` | phandle-array | ✅ | 运动检测中断引脚 |
| `scroll-layers` | array | — | 触发滚动的层编号列表，可多个：`<3 5>` |
| `snipe-layers` | array | — | 触发狙击低速模式的层编号列表 |
| `automouse-layer` | int | — | 触球自动激活的层编号，`-1` 禁用 |

---

## 2. 基础功能开关（conf）

```conf
CONFIG_SPI=y             # 启用 SPI 总线（必须）
CONFIG_INPUT=y           # 启用 ZMK 输入系统（必须）
CONFIG_ZMK_MOUSE=y       # 启用 ZMK 鼠标功能（必须）
CONFIG_PMW3610=y         # 启用 PMW3610 驱动（必须）

CONFIG_PMW3610_SMART_ALGORITHM=y
# 启用 PMW3610 智能追踪算法，改善在光滑/花岗岩等表面的追踪精度
# 默认：y（开启）
```

---

## 3. CPI / 光标速度

CPI（Counts Per Inch）决定光标移动灵敏度，硬件步进为 200，范围 200–3200。

```conf
CONFIG_PMW3610_CPI=800
# 普通移动模式的初始 CPI，默认 800
# 范围：200 ~ 3200，步进 200

CONFIG_PMW3610_CPI_DIVIDOR=1
# 移动模式的软件除数，最终速度 = CPI / DIVIDOR
# 默认：1（不除）；范围：1 ~ 100
# 适合需要比最低 CPI(200) 还慢的场景

CONFIG_PMW3610_CPI_STEP=200
# 每次按 &pms PMW_CPI_INCR / PMW_CPI_DECR 时 CPI 变化量
# 默认：200；范围：200 ~ 3200（必须是 200 的倍数）
```

---

## 4. 狙击模式（Snipe）

在 `snipe-layers` 指定的层上，轨迹球自动切换为低 CPI 精准移动。

```conf
CONFIG_PMW3610_SNIPE_CPI=200
# 狙击模式的初始 CPI，默认 200
# 范围：200 ~ 3200，步进 200

CONFIG_PMW3610_SNIPE_CPI_DIVIDOR=1
# 狙击模式软件除数，默认 1；范围：1 ~ 100

CONFIG_PMW3610_SNIPE_CPI_STEP=200
# 每次按 &pms PMW_SNIPE_CPI_INCR / PMW_SNIPE_CPI_DECR 时 CPI 变化量
# 默认：200；范围：200 ~ 3200
```

---

## 5. 滚动模式（Scroll）

在 `scroll-layers` 指定的层上，轨迹球转动触发滚轮事件。

```conf
CONFIG_PMW3610_SCROLL_TICK=20
# 累积多少传感器 tick 才触发一次滚动事件
# 值越小 → 滚动越灵敏/越快；值越大 → 滚动越迟钝/越慢
# 默认：20

CONFIG_PMW3610_SCROLL_TICK_STEP=5
# 每次按 &pms PMW_SCROLL_SPEED_UP / PMW_SCROLL_SPEED_DOWN 时
# scroll_tick 的变化量，默认 5；范围：1 ~ 100

CONFIG_PMW3610_INVERT_SCROLL_X=n
# 翻转水平滚动方向，默认跟随 PMW3610_INVERT_X

CONFIG_PMW3610_INVERT_SCROLL_Y=n
# 翻转垂直滚动方向，默认跟随 PMW3610_INVERT_Y

CONFIG_PMW3610_SCROLL_ACCELERATION=n
# 启用滚动加速度（速度越快加速越大），默认关闭

CONFIG_PMW3610_SCROLL_ACCELERATION_SENSITIVITY=3
# 滚动加速灵敏度，仅在 SCROLL_ACCELERATION=y 时有效
# 范围：1 ~ 10；值越大加速越猛
```

---

## 6. 自动鼠标层（Automouse）

拨动轨迹球后自动激活指定层，超时后自动退出。

```conf
CONFIG_PMW3610_AUTOMOUSE_TIMEOUT_MS=400
# 停止移动后自动鼠标层持续的时间（毫秒），默认 400

CONFIG_PMW3610_MOVEMENT_THRESHOLD=5
# 触发自动鼠标层所需的最小移动量（tick 数）
# 值越大需要更大幅度移动才触发，防止打字误触
# 默认：5；设为 0 则任意移动即触发
```

overlay 中指定层编号：

```dts
automouse-layer = <4>;  /* 第 4 层为鼠标层 */
```

---

## 7. 球动作层（Ball Action）

在指定层上，把轨迹球的上/下/左/右滑动绑定为任意按键行为（如翻页、切 Tab 等）。

### overlay 配置语法

```dts
trackball: trackball@0 {
    compatible = "zmk,pmw3610";
    /* ... 其他属性 ... */

    /* 球动作子节点，可定义多个（对应不同层） */
    ball_action_layer7 {
        layers   = <7>;                          /* 在第 7 层激活 */
        bindings = <&kp RIGHT> <&kp LEFT>        /* 右滑 / 左滑 */
                   <&kp UP>    <&kp DOWN>;       /* 上滑 / 下滑 */
        tick     = <30>;   /* 触发所需 tick，省略则用 CONFIG_PMW3610_BALL_ACTION_TICK */
        wait-ms  = <0>;    /* 两次触发之间的等待时间（ms） */
        tap-ms   = <10>;   /* 按下到松开的时间间隔（ms） */
    };
};
```

> bindings 顺序固定为：**右 / 左 / 上 / 下**（Right / Left / Up / Down）

### Kconfig 参数

```conf
CONFIG_PMW3610_BALL_ACTION_TICK=20
# 全局默认触发 tick 数，被 overlay 中 tick 属性覆盖
# 值越大需要更大幅度才能触发，默认 20
```

---

## 8. 传感器方向与轴翻转

```conf
# 方向（四选一）
CONFIG_PMW3610_ORIENTATION_0=y    # 不旋转（默认）
CONFIG_PMW3610_ORIENTATION_90=y   # 顺时针旋转 90°
CONFIG_PMW3610_ORIENTATION_180=y  # 旋转 180°
CONFIG_PMW3610_ORIENTATION_270=y  # 顺时针旋转 270°

# 移动轴翻转
CONFIG_PMW3610_INVERT_X=n   # 翻转 X 轴（左右）
CONFIG_PMW3610_INVERT_Y=n   # 翻转 Y 轴（上下）

# 滚动轴翻转（独立控制，默认跟随移动轴）
CONFIG_PMW3610_INVERT_SCROLL_X=n
CONFIG_PMW3610_INVERT_SCROLL_Y=n
```

---

## 9. 鼠标加速度

对移动模式（MOVE）和狙击模式（SNIPE）生效，滚动模式有独立加速设置。

```conf
CONFIG_PMW3610_ACCELERATION_ALGORITHM=0
# 加速算法选择：
#   0 = 关闭（线性，默认）
#   1 = 二次方加速（QMK 风格）：output = x * (1 + |x| / divider)
#       移动幅度越大加速越猛，简单可预测
#   2 = Sigmoid 速度加速：根据移动速度用 S 曲线平滑加速
#       慢速精准、快速流畅

CONFIG_PMW3610_ACCELERATION_SENSITIVITY=3
# 加速灵敏度，仅在算法 1 或 2 时有效
# 范围：1 ~ 100；值越大加速越强，默认 3
```

---

## 10. 低功耗设置

### Kconfig

```conf
CONFIG_PM_DEVICE=y      # 启用 Zephyr 设备电源管理（必须）
CONFIG_PMW3610_PM=y     # 启用 PMW3610 的休眠/唤醒支持
# 系统休眠时传感器进入 Shutdown 模式，唤醒后自动重新初始化

CONFIG_PMW3610_FORCE_AWAKE=n
# 强制传感器始终处于 RUN 状态（禁用自动降频）
# 开启会增加功耗，仅用于调试，默认关闭

# 轮询频率（三选一）
CONFIG_PMW3610_POLLING_RATE_250=y    # 250 Hz（默认，功耗较高）
CONFIG_PMW3610_POLLING_RATE_125=y    # 125 Hz 硬件模式
CONFIG_PMW3610_POLLING_RATE_125_SW=y # 125 Hz 软件实现模式（推荐省电）
```

### 传感器自动降频（REST 模式）

静止一段时间后传感器自动进入低功耗 REST 状态，降低采样频率。

```conf
CONFIG_PMW3610_RUN_DOWNSHIFT_TIME_MS=128
# 从 RUN 切换到 REST1 的等待时间（ms）
# 范围：13 ~ 3264，默认 128

CONFIG_PMW3610_REST1_SAMPLE_TIME_MS=40
# REST1 模式的采样周期（ms），范围 10 ~ 2550，默认 40

CONFIG_PMW3610_REST1_DOWNSHIFT_TIME_MS=9600
# 从 REST1 切换到 REST2 的等待时间（ms），默认 9600

CONFIG_PMW3610_REST2_SAMPLE_TIME_MS=0
# REST2 模式采样周期（ms），0 表示不进入 REST2
# 最小有效值 10，范围 0 ~ 2550

CONFIG_PMW3610_REST2_DOWNSHIFT_TIME_MS=0
# 从 REST2 切换到 REST3 的等待时间（ms），0 表示不进入 REST3

CONFIG_PMW3610_REST3_SAMPLE_TIME_MS=0
# REST3 模式采样周期（ms），0 表示不进入 REST3
# 最小有效值 10，范围 0 ~ 2550
```

### overlay 中的 SPI 引脚低功耗

`spi1_sleep` 组中加 `low-power-enable` 可让 SPI 引脚在系统休眠时进入高阻态，避免漏电：

```dts
spi1_sleep: spi1_sleep {
    group1 {
        psels = <NRF_PSEL(SPIM_SCK,  1, 13)>,
                <NRF_PSEL(SPIM_MOSI, 0, 10)>,
                <NRF_PSEL(SPIM_MISO, 0, 10)>;
        low-power-enable;   /* ← 关键：休眠时引脚不拉电流 */
    };
};
```

---

## 11. 运行时动态调节键值（&pms）

在 keymap 中使用 `&pms <参数>` 绑定按键，运行时即时生效，无需重新烧录。

### 前提：在 overlay 或 dtsi 中引入行为节点

```dts
#include <behaviors/pmw3610_setting.dtsi>
/* 或者手动声明： */
/ {
    behaviors {
        pms: behavior_pmw3610_setting {
            compatible = "zmk,behavior-pmw3610-setting";
            #binding-cells = <1>;
        };
    };
};
```

### 所有可用参数

需要在 keymap 头部引入：
```c
#include <dt-bindings/zmk/pmw3610_settings.h>
```

| 参数 | 值 | 功能 | 步进来源 |
|------|----|------|----------|
| `PMW_CPI_INCR` | 0 | 移动 CPI +1 步 | `CONFIG_PMW3610_CPI_STEP`（默认 200） |
| `PMW_CPI_DECR` | 1 | 移动 CPI -1 步 | `CONFIG_PMW3610_CPI_STEP`（默认 200） |
| `PMW_SNIPE_CPI_INCR` | 2 | 狙击 CPI +1 步 | `CONFIG_PMW3610_SNIPE_CPI_STEP`（默认 200） |
| `PMW_SNIPE_CPI_DECR` | 3 | 狙击 CPI -1 步 | `CONFIG_PMW3610_SNIPE_CPI_STEP`（默认 200） |
| `PMW_SCROLL_SPEED_UP` | 4 | 滚动速度加快 | `CONFIG_PMW3610_SCROLL_TICK_STEP`（默认 5） |
| `PMW_SCROLL_SPEED_DOWN` | 5 | 滚动速度减慢 | `CONFIG_PMW3610_SCROLL_TICK_STEP`（默认 5） |

### keymap 使用示例

```c
#include <dt-bindings/zmk/pmw3610_settings.h>

/ {
    keymap {
        compatible = "zmk,keymap";

        mouse_layer {
            bindings = <
                &pms PMW_CPI_INCR         /* 移动速度加快 */
                &pms PMW_CPI_DECR         /* 移动速度减慢 */
                &pms PMW_SNIPE_CPI_INCR   /* 狙击速度加快 */
                &pms PMW_SNIPE_CPI_DECR   /* 狙击速度减慢 */
                &pms PMW_SCROLL_SPEED_UP  /* 滚动速度加快 */
                &pms PMW_SCROLL_SPEED_DOWN /* 滚动速度减慢 */
            >;
        };
    };
};
```

### 步进自定义

```conf
CONFIG_PMW3610_CPI_STEP=400          # 每次调节移动 CPI 变化 400
CONFIG_PMW3610_SNIPE_CPI_STEP=200    # 每次调节狙击 CPI 变化 200
CONFIG_PMW3610_SCROLL_TICK_STEP=10   # 每次调节滚动 tick 变化 10
```

---

## 12. 完整配置示例

### keyball61_right.conf（参考）

```conf
# === 基础依赖 ===
CONFIG_SPI=y
CONFIG_INPUT=y
CONFIG_ZMK_MOUSE=y
CONFIG_PMW3610=y

# === 低功耗 ===
CONFIG_PM_DEVICE=y
CONFIG_PMW3610_PM=y
CONFIG_PMW3610_POLLING_RATE_125_SW=y   # 软件 125Hz，省电
CONFIG_PMW3610_RUN_DOWNSHIFT_TIME_MS=3264
CONFIG_PMW3610_REST1_SAMPLE_TIME_MS=20

# === 移动 CPI ===
CONFIG_PMW3610_CPI=200
CONFIG_PMW3610_CPI_DIVIDOR=1
CONFIG_PMW3610_CPI_STEP=200

# === 狙击模式 ===
CONFIG_PMW3610_SNIPE_CPI=400
CONFIG_PMW3610_SNIPE_CPI_DIVIDOR=4
CONFIG_PMW3610_SNIPE_CPI_STEP=200

# === 滚动 ===
CONFIG_PMW3610_SCROLL_TICK=32
CONFIG_PMW3610_SCROLL_TICK_STEP=5
CONFIG_PMW3610_INVERT_SCROLL_X=y
CONFIG_PMW3610_INVERT_SCROLL_Y=n

# === 方向 ===
CONFIG_PMW3610_ORIENTATION_180=y

# === 自动鼠标层 ===
CONFIG_PMW3610_AUTOMOUSE_TIMEOUT_MS=700
CONFIG_PMW3610_MOVEMENT_THRESHOLD=0

# === 智能算法 ===
CONFIG_PMW3610_SMART_ALGORITHM=y
```

### keyball61_right.overlay（参考）

```dts
&pinctrl {
    spi1_default: spi1_default {
        group1 {
            psels = <NRF_PSEL(SPIM_SCK, 1, 13)>,
                    <NRF_PSEL(SPIM_MOSI, 0, 10)>,
                    <NRF_PSEL(SPIM_MISO, 0, 10)>;
        };
    };
    spi1_sleep: spi1_sleep {
        group1 {
            psels = <NRF_PSEL(SPIM_SCK, 1, 13)>,
                    <NRF_PSEL(SPIM_MOSI, 0, 10)>,
                    <NRF_PSEL(SPIM_MISO, 0, 10)>;
            low-power-enable;
        };
    };
};

&spi1 {
    status = "okay";
    compatible = "nordic,nrf-spim";
    pinctrl-0 = <&spi1_default>;
    pinctrl-1 = <&spi1_sleep>;
    pinctrl-names = "default", "sleep";
    cs-gpios = <&gpio0 9 GPIO_ACTIVE_LOW>;

    trackball: trackball@0 {
        status = "okay";
        compatible = "zmk,pmw3610";
        reg = <0>;
        spi-max-frequency = <2000000>;
        irq-gpios = <&gpio1 11 (GPIO_ACTIVE_LOW | GPIO_PULL_UP)>;
        automouse-layer = <4>;
        scroll-layers = <5>;
        snipe-layers  = <6>;

        /* 球动作示例：第 7 层，上下左右滑动触发方向键 */
        ball_action_nav {
            layers   = <7>;
            bindings = <&kp RIGHT> <&kp LEFT> <&kp UP> <&kp DOWN>;
            tick     = <30>;
            tap-ms   = <10>;
            wait-ms  = <0>;
        };
    };
};

/ {
    trackball_listener {
        compatible = "zmk,input-listener";
        device = <&trackball>;
    };
};
```

---

## 参数速查表

| 参数 | 默认值 | 范围 | 说明 |
|------|--------|------|------|
| `PMW3610_CPI` | 800 | 200–3200 | 移动模式初始 CPI |
| `PMW3610_CPI_DIVIDOR` | 1 | 1–100 | 移动模式软件除数 |
| `PMW3610_CPI_STEP` | 200 | 200–3200 | 动态调节步进 |
| `PMW3610_SNIPE_CPI` | 200 | 200–3200 | 狙击模式初始 CPI |
| `PMW3610_SNIPE_CPI_DIVIDOR` | 1 | 1–100 | 狙击模式软件除数 |
| `PMW3610_SNIPE_CPI_STEP` | 200 | 200–3200 | 狙击 CPI 调节步进 |
| `PMW3610_SCROLL_TICK` | 20 | 1–200 | 滚动触发 tick 阈值 |
| `PMW3610_SCROLL_TICK_STEP` | 5 | 1–100 | 滚动速度调节步进 |
| `PMW3610_SCROLL_ACCELERATION` | n | y/n | 滚动加速开关 |
| `PMW3610_SCROLL_ACCELERATION_SENSITIVITY` | 3 | 1–10 | 滚动加速灵敏度 |
| `PMW3610_ACCELERATION_ALGORITHM` | 0 | 0–2 | 移动加速算法 |
| `PMW3610_ACCELERATION_SENSITIVITY` | 3 | 1–100 | 移动加速灵敏度 |
| `PMW3610_AUTOMOUSE_TIMEOUT_MS` | 400 | — | 自动鼠标层超时（ms） |
| `PMW3610_MOVEMENT_THRESHOLD` | 5 | 0–∞ | 触发自动层的最小位移 |
| `PMW3610_BALL_ACTION_TICK` | 20 | — | 球动作全局默认 tick |
| `PMW3610_RUN_DOWNSHIFT_TIME_MS` | 128 | 13–3264 | RUN→REST1 延迟（ms） |
| `PMW3610_REST1_SAMPLE_TIME_MS` | 40 | 10–2550 | REST1 采样周期（ms） |
| `PMW3610_REST1_DOWNSHIFT_TIME_MS` | 9600 | — | REST1→REST2 延迟（ms） |
| `PMW3610_REST2_SAMPLE_TIME_MS` | 0 | 0–2550 | REST2 采样周期（0=不进入） |
| `PMW3610_REST2_DOWNSHIFT_TIME_MS` | 0 | — | REST2→REST3 延迟（0=不进入） |
| `PMW3610_REST3_SAMPLE_TIME_MS` | 0 | 0–2550 | REST3 采样周期（0=不进入） |

---

## 附：与 keyball61 keymap 的对应关系

你当前的 keymap 层定义如下，与 overlay 中的层编号完全对应：

| 宏 | 层编号 | 说明 |
|----|--------|------|
| `DEFAULT` | 0 | 默认打字层 |
| `NUM` | 1 | 数字层 |
| `SYM` | 2 | 蓝牙/符号层（blue_layer） |
| `FUN` | 3 | 功能键层 |
| `MOUSE` | 4 | 自动鼠标层（`automouse-layer = <4>`） |
| `SCROLL` | 5 | 滚动层（`scroll-layers = <5>`） |
| `SNIPE` | 6 | 狙击层（`snipe-layers = <6>`） |

当前 keymap 中已定义的宏快捷方式（default_layer 里使用）：

```c
#define U_PMS_CPI_I &pms PMW_CPI_INCR   /* 移动 CPI + */
#define U_PMS_CPI_D &pms PMW_CPI_DECR   /* 移动 CPI - */
```

**建议补充的宏**（可直接加到 keymap 头部）：

```c
#define U_PMS_SNIPE_I  &pms PMW_SNIPE_CPI_INCR    /* 狙击 CPI + */
#define U_PMS_SNIPE_D  &pms PMW_SNIPE_CPI_DECR    /* 狙击 CPI - */
#define U_PMS_SCR_U    &pms PMW_SCROLL_SPEED_UP   /* 滚动速度加快 */
#define U_PMS_SCR_D    &pms PMW_SCROLL_SPEED_DOWN /* 滚动速度减慢 */
```

这些宏可以放进 `mouse_layer` 或 `snipe_layer` 的空 `&trans` 位置，方便随时在键盘上调速。
