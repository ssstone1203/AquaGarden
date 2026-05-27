# RA8P1学习笔记

这篇笔记主要是记录RA8P1的特性

## Helium技术

Helium单指令多数据。8个128位的Q寄存器。128位可以拆成16个8位整数，8个16位整数

![Imgur](https://imgur.com/Fw96b0k.png)

## AMP核间通信

CPU0(CM85) / CPU1(CM33)

上电复位后先启动CPU0，进入hal_entry后需要显式调用R_BSP_SecondaryCoreStart()来启动CPU1，CPU1复位跳转到hal_entry

### 硬件方式

ra8p1通过IPC实现CPU之间的通信

![Imgur](https://imgur.com/LUsusRf.png)

### 软件方式

使用共享内存

## FSP开发配置

![Imgur](https://imgur.com/moN3JZ5.png)

## 在RA8P1上部署AI

BYOM(bring your own model)开发模式

RUHMI工具：对初学者而言简单易用，对专家而言强大高效

Renesas RUHMI（Robust Unified Heterogeneous Model Integration）框架是一套用于加速 Renesas MCU/MPU 产品 AI 应用开发的创新工具集。 它能够在数分钟内生成高度优化的模型，并最大限度发挥 Renesas 嵌入式处理器的性能。