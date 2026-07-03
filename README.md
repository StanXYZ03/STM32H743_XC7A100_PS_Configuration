# STM32H743_XC7A100_PS_Configuration

## 1. 项目概述

本目录是基于 **STM32H743** 的综合实验与控制主工程，当前工程已经融合了以下几条主线能力：

- FPGA 远程配置
  - 支持通过 USB CDC 与板端 UI 两种入口触发配置流程
  - 支持 Slave Serial / JTAG-SRAM / JTAG-Flash 三种配置模式
- emWin 图形界面与实体交互
  - 使用 LCD + SDRAM + LTDC + DMA2D 构建人机界面
  - 使用 EC1 / EC2 编码器完成菜单导航、模式进入、退出与中断配置
- 鼠标键盘数据链路
  - STM32 通过 SPI1 从 FPGA 侧读取键鼠打包数据
  - 再通过 USB CDC 输出给上位机
  - 同时支持 LCD 上的 Remote Control 页面进行坐标映射显示
- 以太网实验链路
  - 已建立 `ETHDefaultTask` 与 `Bsp_ETH` 最小数据通路
  - 当前主要用于实验验证与后续扩展
- Lattice / Xilinx 辅助工程与桥接逻辑
  - 工程目录中保留了 FPGA 桥接、GPIO 探针、Vivado/Lattice 相关测试工程和脚本

本项目已经不再是单一 CubeMX 空工程，而是一个经过多轮融合的 **主控 + UI + FPGA 配置 + 外设实验平台**。

---

## 2. 顶层目录结构

以下只列出当前目录下最重要、最常用的部分：

```text
STM32H743_XC7A100_PS_Configuration/
├─ Core/                     STM32 主控制代码（CubeMX 生成 + 手工业务逻辑）
├─ User/                     BSP、UI、自定义平台层代码
├─ Drivers/                  HAL、CMSIS、BSP 组件驱动
├─ Middlewares/              FreeRTOS、LwIP、USB Device、emWin 等中间件
├─ USB_DEVICE/               USB CDC 设备层工程文件
├─ LWIP/                     以太网/LwIP 工程适配层
├─ emWin/                    emWin 显示驱动、配置与图形库
├─ MDK-ARM/                  Keil 主工程文件、启动文件、编译输出
├─ Keil_H743_Project/        待融合/参考用的独立 H743 SPI+LCD 实验工程
├─ Mouse_Key/                Xilinx 侧键鼠逻辑工程
├─ spi/                      参考 SPI 从设备/协议实验代码
├─ STM32-FPGA/               更早期的 STM32-FPGA 参考工程
├─ Confg-Flash/              配置/Flash 相关辅助产物与脚本
├─ Host/                     上位机/主机侧相关内容（如有）
├─ vivado_bridge/            Vivado 桥接工程与脚本
├─ vivado_gpio_probe/        GPIO 探针测试工程
├─ STM32_impl/               Lattice 编译输出
├─ *.ioc                     CubeMX 工程配置文件
└─ README.md                 当前说明文档
```

---

## 3. Keil 主工程中各分组的作用

主工程大致分为以下几个分组：

### 3.1 `Application/MDK-ARM`

- `startup_stm32h743xx.s`
- MCU 上电后的启动入口、栈堆定义、中断向量表基础内容

### 3.2 `Application/User/Core`

位于 `Core/Src/`，是主控基础逻辑与 CubeMX 生成代码的核心区域：

- `main.c`
  - 工程总入口
  - 负责上电初始化顺序、外设初始化、FreeRTOS 启动前平台准备
- `freertos.c`
  - FreeRTOS 任务创建与调度入口
- `gpio.c / dma.c / dma2d.c / fmc.c / ltdc.c / spi.c`
  - CubeMX 生成的外设初始化代码
- `stm32h7xx_it.c`
  - 中断服务函数
- `stm32h7xx_hal_msp.c`
  - HAL 外设底层 MSP 初始化
- `sdram.c`
  - SDRAM 额外初始化/命令序列补充
- `memorymap.c`
  - 地址映射辅助
- `fpga_config.c`
  - FPGA 配置底层执行逻辑

### 3.3 `Tasks`

位于 `Core/Src/`，负责主工程不同业务线程：

- `FPGAConfigDefaultTask.c`
  - FPGA 配置主任务
  - 负责 USB/板端 UI 配置会话、bin 接收、配置启动、中断配置等逻辑
- `ewWinDefaultTask.c`
  - emWin 图形任务
- `mousekeyDefaultTask.c`
  - 键鼠数据采集与上位机透传任务
  - 通过 SPI1 与 FPGA 交互，USB CDC 向上位机发送数据包
- `ETHDefaultTask.c`
  - 以太网实验/透传任务
- `ExpH743SpiBridge.c`
  - 从 `Keil_H743_Project` 中抽出的 SPI1 12 字节帧采集与解析后端模块
  - 当前仅完成底层迁移，尚未完全接管主工程显示/UI 流程

### 3.4 `Bsp`

位于 `User/bsp/src/`，是硬件板级支持层：

- `bsp_lcd_rgb.c`
  - RGB LCD 面板初始化、寄存器下发、背光控制
- `bsp_fmc_sdram.c`
  - SDRAM 初始化与测试
- `bsp_ui_io_expander.c`
  - 编码器/按键扩展器访问
- `bsp_dwt.c`
  - 微秒级延时支持
- `bsp_led.c`
  - 板载 LED 控制
- `Bsp_ETH.c`
  - 以太网实验配置与 GPIO 数据采样
- `retarget_no_semihost.c`
  - 调试输出/运行库适配辅助
- `bsp.c`
  - 通用 BSP 基础函数

### 3.5 `UI`

位于 `User/ui/`，是当前主工程的人机界面逻辑层：

- `MainTask.c`
  - UI 主调度与页面刷新控制
- `ui_nav.c`
  - 菜单导航、EC1/EC2 交互逻辑
- `ui_screens.c`
  - 各页面具体绘制逻辑
- `UI_WaveformCtrl.c`
  - 波形相关 UI 控件/逻辑
- `dma2d_wave.c`
  - DMA2D 辅助绘图
- `lcd_rotate_profile.c`
  - LCD 旋转/映射配置

### 3.6 `emWin`

位于 `emWin/`，是当前显示栈的底层图形库：

- `GUIConf.c`
  - emWin 配置
- `GUI_X_FreeRTOS.c`
  - emWin 与 FreeRTOS 适配
- `LCDConf_Lin_Template.c`
  - 显示驱动核心、缓冲区和旋转/刷新实现
- `STemWin_CM7_OS_wc16_ot_ARGB.a`
  - emWin 静态库

### 3.7 `USB_DEVICE`

USB CDC 设备栈：

- `usb_device.c`
- `usbd_desc.c`
- `usbd_cdc_if.c`
- `usbd_conf.c`
- `usbd_core.c / usbd_ctlreq.c / usbd_ioreq.c / usbd_cdc.c`

主要作用：

- 作为上位机与 STM32 之间的数据链路
- 用于 FPGA 远程配置命令与 bin 文件传输
- 也用于键鼠数据透传输出

### 3.8 `LWIP` 与 `Middlewares/LwIP`

- `LWIP/App/lwip.c`
- `LWIP/Target/ethernetif.c`
- `Middlewares/Third_Party/LwIP/...`

主要作用：

- 提供以太网和 LwIP 协议栈基础能力
- 当前主工程中主要被 `ETHDefaultTask` 用于实验验证和后续扩展

---

## 4. 当前工程实现现状

### 4.1 已基本完成并可用的部分

1. FPGA 配置流程
- 板端 UI 可进入 FPGA Configuration 页面
- 支持三种配置模式
- 支持 USB CDC 收发配置命令、bin 文件和启动命令
- 支持配置流程中断与会话取消

2. emWin 界面与编码器交互
- 主菜单和多个子页面可正常显示
- EC1/EC2 菜单导航和返回逻辑已基本打通
- `Remote Control` 页面已实现鼠标坐标映射显示

3. 键鼠数据链路
- STM32 可从 FPGA 侧读取键鼠数据
- 通过 USB CDC 发往上位机
- LCD 上可显示缩放后的鼠标位置

4. LCD / SDRAM / LTDC
- 屏幕当前已能正常工作
- SDRAM 初始化与帧缓冲链路已打通
- 局部刷新与旋转映射问题已做过多轮修正

### 4.2 已接入但仍在持续完善的部分

1. 以太网功能
- `ETHDefaultTask` 与 `Bsp_ETH` 已接入主工程
- LwIP 已能编译通过并参与系统构建
- 当前仍偏实验/验证阶段，完整业务协议尚未完全固化

2. `Keil_H743_Project` 的迁移
- 该工程本质上是一个 **SPI1 + LCD + SDRAM 的独立实验工程**
- 当前只将其最核心的 **SPI1 帧采集/解析后端** 提炼为 `ExpH743SpiBridge`
- 原工程那套整页 LCD 展示逻辑、裸机主循环 UI 还未整体接管当前主工程

### 4.3 当前需要特别注意的问题

1. 资源冲突
- `Keil_H743_Project` 的 SPI1 数据链路与当前 `mousekeyDefaultTask` 都依赖 `SPI1`
- 若后续继续融合，必须先明确任务仲裁或模式独占关系

2. > [!IMPORTANT]
   >
   > **CubeMX 重新生成后注意要把FreeRTOSConfig.h的91行改为**
   >
   > **#define INCLUDE_vTaskDelayUntil              1**
---

## 5. 构建与调试说明

### 5.1 CubeMX

主工程 CubeMX 配置文件：

- `STM32H743_XC7A100_PS_Configuration.ioc`

说明：

- 当前主工程的大多数基础外设都由这份 `.ioc` 生成
- 但工程中同时存在较多手工融合逻辑，重新生成代码后需要人工复核

### 5.2 Keil 工程入口

主工程 Keil 工程文件：

- `MDK-ARM/STM32H743_XC7A100_PS_Configuration.uvprojx`

### 5.3 推荐阅读顺序

如果第一次接手该工程，建议按下面顺序阅读：

1. `Core/Src/main.c`
2. `Core/Src/freertos.c`
3. `Core/Src/FPGAConfigDefaultTask.c`
4. `Core/Src/mousekeyDefaultTask.c`
5. `User/ui/MainTask.c`
6. `User/ui/ui_nav.c`
7. `User/ui/ui_screens.c`
8. `User/bsp/src/bsp_lcd_rgb.c`
9. `emWin/DisplayDriver/LCDConf_Lin_Template.c`
10. `Core/Src/ETHDefaultTask.c`
11. `Core/Src/ExpH743SpiBridge.c`

---

## 6. 后续建议

当前主工程已经具备较强的实验平台属性。后续如果继续扩展，建议按以下原则推进：

1. 先明确“功能主线”再做融合
- 避免多个实验工程同时争夺同一外设

2. 对所有“独占型外设”建立清单
- 尤其是 SPI1、SPI4、LTDC、DMA、USB、ETH

3. 新迁移功能先拆成后端模块再接 UI
- 先迁数据链路，再迁显示层
- 当前 `ExpH743SpiBridge` 就是按这个思路处理的第一步

