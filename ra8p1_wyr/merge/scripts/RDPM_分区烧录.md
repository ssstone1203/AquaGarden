# merge 扩大 MRAM 分区 — 详细操作指南

## RDPM 是什么？

**Renesas Device Partition Manager（RDPM）** 是瑞萨官方工具，用来配置芯片的 **TrustZone 内存分区**（哪一段 MRAM 是 Secure、多大等）。

- 不是单独安装的 exe，而是 **内嵌在 RASC / e² studio** 里
- 你电脑上已有 RASC：`D:\Renesas\RA_new\eclipse\rascc.exe`
- Keil 里 **Download 只烧代码**，**不会**自动改分区；32 KB 边界是芯片 OTP/分区寄存器决定的，必须 RDPM（或 RFP）改一次

---

## 有没有更简单的方法？

| 方法 | 难度 | 说明 |
|------|------|------|
| **A. RDPM「恢复出厂分区」** | ★ 最简单 | 推荐先试。把整片代码 MRAM 标为 Secure，通常远大于 32 KB |
| **B. RDPM 加载 merge.rpd** | ★★ | 按工程精确设为 512 KB（0x80000） |
| **C. Renesas Flash Programmer (RFP)** | ★★★ | 独立 GUI，功能同 RDPM，需另下载 |
| Keil 直接 Download | ✗ 不行 | 只写 hex，不改分区 |
| J-Link 脚本 | ✗ 不行 | 无法写 RA8P1 分区寄存器 |

**建议顺序：先试方法 A；若 Rebuild 后仍读不到 `0x02008000`，再用方法 B。**

---

## 方法 A：恢复出厂分区（推荐先试）

### 1. 打开 RDPM

任选一种：

**方式 1 — 双击脚本（最简单）**

```cmd
cd E:\AquaGarden\hardware\ra8p1_wyr\merge
scripts\run_rdpm.cmd
```

**方式 2 — Keil 菜单（需先配置一次）**

1. Keil：**Project → Options for Target → Utilities → Settings**
2. **Tools → Configure Tools… → New**
3. 填写：
   - Menu Text: `Device Partition Manager`
   - Command: `D:\Renesas\RA_new\eclipse\rascc.exe`
   - Arguments: `-application com.renesas.cdt.ddsc.dpm.ui.dpmapplication configuration.xml "$L%L"`
   - Initial Folder: `$P`
4. 以后：**Tools → Device Partition Manager**

**方式 3 — e² studio**

Run → Renesas Debug Tools → Renesas Device Partition Manager

### 2. 连接设置

1. 板卡 USB-C 接 PC，J-Link 驱动正常（与 Keil 调试相同）
2. RDPM 窗口里：
   - **Connection Type**: **SWD**（不要 JTAG）
   - **Device**: R7KA8P1 相关型号（若可选手动选 CPU0）

### 3. 执行「Initialize device」

1. 勾选 **Initialize device back to factory default**（或 **Initialize device**）
   - 作用：清除之前 soil_sensor 等工程留下的 **32 KB 分区限制**，整片代码 MRAM 标为 Secure
2. 点击 **Run** / **Execute**
3. 等待成功提示
4. **断电再上电**（或拔插 USB-C），再进入下一步

> 此操作会改写芯片分区 OTP，**不可逆**。开发板首次使用通常没问题。

### 4. 烧录 merge 固件

1. Keil **Rebuild** `merge.uvprojx`
2. **Download**（F8）或 `JLinkExe -CommandFile scripts\flash_code.jlink`

### 5. 验证

```cmd
JLinkExe -CommandFile scripts\probe_flash.jlink
```

- `0x02008000` 应能读出数据（不是全 `AA`）
- Watch：`g_startup_probe_high` = **10**，`g_task_hb_usb` 递增

---

## 方法 B：加载 merge.rpd（精确 512 KB）

若方法 A 后仍超 32 KB 边界失败，用工程生成的分区文件：

### 1. 确认 .rpd 已生成

Keil Rebuild 后检查 `Objects\merge.rpd` 第一行附近：

```text
FLASH_CPU0_S_SIZE=0x80000
```

若不是，AfterMake 会自动跑 `patch_merge_rpd.py`；也可手动：

```cmd
python scripts\patch_merge_rpd.py Objects\merge.rpd
```

### 2. 打开 RDPM

同方法 A 的 `scripts\run_rdpm.cmd`

### 3. 加载并烧录

1. **File → Open** → 选 `Objects\merge.rpd`
2. 确认 **FLASH_CPU0_S** = **0x80000（512 KB）**
3. **Program** / **Write partition**
4. 断电上电 → Keil Download

---

## 方法 C：Renesas Flash Programmer (RFP)

1. 下载：[Renesas Flash Programmer](https://www.renesas.com/en/software-tool/renesas-flash-programmer-programming-gui)
2. 连接 SWD，选择 RA8P1
3. **Partition Boundaries** 选项卡 → 导入 `merge.rpd` 或手动设 Secure Code = 512 KB
4. Program → 再 Keil Download

---

## 常见问题

**Q: 找不到 RDPM 菜单？**  
用 `scripts\run_rdpm.cmd`，不依赖 Keil 配置。

**Q: RDPM 连接失败？**  
关闭 Keil 调试会话（避免 J-Link 被占用）→ 只留 RDPM 连接 → 确认 SWD。

**Q: Initialize 和加载 rpd 区别？**  
Initialize = 出厂默认（整片 Secure，适合开发）；rpd = 与 solution.xml 完全一致（适合量产/多核）。

**Q: 还要 run_gensolutionbundle.cmd 吗？**  
方法 A 通常 **不需要**。方法 B 或后续改 solution.xml 时需要。

---

## 烧录后 Watch 参考

```
g_startup_probe_high    → 10
g_task_hb_sensor        → 递增
g_task_hb_actuator      → 递增
g_task_hb_usb           → 递增
g_fault_type            → 0
```
