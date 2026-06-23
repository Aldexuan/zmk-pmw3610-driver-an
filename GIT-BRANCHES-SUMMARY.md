# Git 分支总结

**最后更新**: 2026-06-18  
**仓库**: https://github.com/Aldexuan/zmk-pmw3610-driver-an

---

## 📋 当前可用的分支

### 1. `hybrid-power-management` ⭐ **最新 - 推荐测试**

**状态**: ✅ 已推送到 GitHub  
**基于**: commit 1bc8eec (稳定版本)  
**提交**: cb03f8b

**核心特性**:
- 使用 ZMK 活动状态监听器替代 PM_DEVICE
- 三级功耗管理（ACTIVE/IDLE/SLEEP）
- 完整 REST1 → REST2 → REST3 降频链
- SLEEP 时释放 GPIO，但不调用 SHUTDOWN

**预期效果**:
- 深睡眠功耗: ~300μA（与稳定版本相同）
- 平均功耗: ~0.46mA（提升 52%）
- 续航: ~12.5天（提升 2.08倍）
- 唤醒时间: IDLE→ACTIVE ~10ms, SLEEP→ACTIVE ~50ms

**使用方法**:
```bash
git checkout hybrid-power-management
# 在你的 zmk-config 中重新编译固件
```

**文档**:
- `HYBRID-STRATEGY-README.md` - 详细使用说明
- `docs/混合策略实施记录.md` - 实施记录
- `docs/混合策略-ZMK监听器三级功耗管理方案.md` - 设计方案

---

### 2. `no-shutdown-stable` ✅ **稳定版本**

**状态**: ✅ 已推送到 GitHub  
**基于**: commit 66019b3  
**提交**: 1bc8eec

**核心特性**:
- 移除了 SHUTDOWN 代码（避免功耗升高问题）
- 保留所有用户功能（CPI 调节、Snipe、Scroll、持久化）
- 使用 PM_DEVICE 框架
- 深睡眠时释放 GPIO

**功耗表现**:
- 深睡眠: 稳定 300μA
- 平均功耗: ~0.96mA
- 续航: ~6天
- **关键**: 功耗不会随时间升高（已验证）

**使用方法**:
```bash
git checkout no-shutdown-stable
# 在你的 zmk-config 中重新编译固件
```

**适用场景**:
- 如果混合策略测试不理想，使用这个分支
- 已知稳定，功耗可靠
- 作为功耗测试的对照组

**文档**:
- `RECOVERY-NOTE.md` - 回退说明
- `GIT-BACKUP-INFO.md` - 备份信息

---

### 3. `main` 📦 **原始主分支**

**状态**: 包含 SHUTDOWN 代码（有功耗升高问题）  
**提交**: 220b20b

**问题**:
- 初始功耗 300μA，2-3 小时后升至 700-800μA
- 不推荐使用

**仅用于**:
- 查看原始代码
- 对比分析

---

## 🎯 推荐使用策略

### 方案 A：积极测试新方案

1. **先测试混合策略** (`hybrid-power-management`)
   ```bash
   git checkout hybrid-power-management
   ```
   
2. **进行功耗测试**
   - 测试各状态功耗（ACTIVE/IDLE/SLEEP）
   - 长期监测 24-48 小时
   - 验证功耗稳定性

3. **如果测试通过**
   - 可以合并到 main 或创建新的稳定分支
   - 享受 2 倍续航提升 🎉

4. **如果测试不理想**
   - 切回 `no-shutdown-stable`
   - 报告问题，我们继续优化

---

### 方案 B：保守稳定使用

1. **直接使用稳定版本** (`no-shutdown-stable`)
   ```bash
   git checkout no-shutdown-stable
   ```

2. **已知功耗表现**
   - 深睡眠 300μA，稳定不升高
   - 续航约 6 天（500mAh 电池）

3. **等待混合策略验证**
   - 等其他用户测试混合策略
   - 确认稳定后再升级

---

## 📊 分支对比表

| 特性 | `main` | `no-shutdown-stable` | `hybrid-power-management` |
|-----|--------|---------------------|--------------------------|
| **功耗管理** | PM_DEVICE + SHUTDOWN | PM_DEVICE + GPIO释放 | ZMK监听器 + 三级管理 |
| **深睡眠功耗** | 300μA→700μA ❌ | 300μA ✅ | 300μA ✅ |
| **功耗稳定性** | 不稳定 | 稳定 | 预期稳定（待测） |
| **平均功耗** | ~0.96mA | ~0.96mA | ~0.46mA 🚀 |
| **续航时间** | ~6天 | ~6天 | ~12.5天 🚀 |
| **唤醒时间** | ~300ms | ~300ms | ~50ms ⚡ |
| **可靠性** | 低（有bug） | 高 | 预期高（待测） |
| **推荐使用** | ❌ 不推荐 | ✅ 稳定可靠 | ⭐ 推荐测试 |

---

## 🔧 如何切换分支

### 在驱动仓库中切换

```bash
cd /path/to/zmk-pmw3610-driver-an

# 查看所有分支
git branch -a

# 切换到混合策略
git checkout hybrid-power-management

# 或切换到稳定版本
git checkout no-shutdown-stable

# 查看当前分支
git branch
```

### 在你的 ZMK 配置中更新

如果你的 `zmk-config/config/west.yml` 中指定了分支：

```yaml
manifest:
  remotes:
    - name: aldexuan
      url-base: https://github.com/Aldexuan
  projects:
    - name: zmk-pmw3610-driver-an
      remote: aldexuan
      revision: hybrid-power-management  # 改这里
      path: modules/drivers/pmw3610
```

然后：
```bash
cd /path/to/zmk-config
west update
west build -p -b <your_board>
```

---

## 📝 Git 历史

```
cb03f8b (hybrid-power-management) feat: 实施混合功耗管理策略
    ↑ 最新实施，使用 ZMK 监听器 + 三级降频
    |
1bc8eec (no-shutdown-stable) 回退到稳定版本：移除SHUTDOWN代码
    ↑ 稳定版本，功耗 300μA 不升高
    |
66019b3 test
    ↑ 原始稳定点（用户的所有功能都在）
    |
220b20b (main) pm
    ↑ 包含 SHUTDOWN 代码（有功耗升高问题）
```

---

## 🐛 问题追踪

### 已知问题

1. **main 分支**: SHUTDOWN 导致功耗升高（300μA → 700-800μA）
   - **状态**: 已放弃，不再使用
   - **解决**: 创建 `no-shutdown-stable` 和 `hybrid-power-management`

### 待验证问题

1. **hybrid-power-management**: 功耗和稳定性需要在实际硬件上测试
   - **状态**: 代码实施完成，待测试
   - **关键测试**: 24 小时功耗监测，确认不升高

---

## 📞 如何反馈问题

如果在测试中发现问题：

1. **记录详细信息**
   - 使用的分支和 commit
   - 功耗测量数据
   - 日志输出
   - 复现步骤

2. **创建 GitHub Issue**
   - 仓库: https://github.com/Aldexuan/zmk-pmw3610-driver-an/issues

3. **提供对比数据**
   - 与 `no-shutdown-stable` 对比
   - 不同状态下的功耗

---

## 🎉 总结

- **生产使用**: 使用 `no-shutdown-stable` ✅
- **测试新功能**: 使用 `hybrid-power-management` ⭐
- **避免使用**: `main` 分支 ❌

**下一步**: 在实际硬件上测试混合策略，验证功耗和稳定性！

---

**祝你使用愉快！** 🚀
