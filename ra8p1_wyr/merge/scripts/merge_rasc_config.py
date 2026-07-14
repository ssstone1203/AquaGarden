#!/usr/bin/env python3
"""Merge module RASC configuration.xml fragments into merge/configuration.xml."""
import copy
import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

BASE = Path(r"E:/AquaGarden/hardware/ra8p1_wyr")
MERGE = BASE / "merge"

root = ET.parse(BASE / "soil_sensor/configuration.xml").getroot()

# SCI9 uses SCICLK from PLL1R. USB HS Host needs PLL2 + UCLK/USB60CLK (see usb_light).
clk_cfg = root.find("raClockConfiguration")
if clk_cfg is not None:
    clk_usb = {
        "board.clock.pll2.source": "board.clock.pll2.source.xtal",
        "board.clock.uck.source": "board.clock.uck.source.pll2r",
        "board.clock.u60ck.source": "board.clock.u60ck.source.pll2r",
        "board.clock.sciclk.source": "board.clock.sciclk.source.pll1r",
    }
    for node in clk_cfg.findall("node"):
        nid = node.get("id")
        if nid in clk_usb:
            node.set("option", clk_usb[nid])

comp_sel = root.find("raComponentSelection")
extra_comps = """
    <component apiversion="" class="HAL Drivers" condition="" group="all" subgroup="r_iic_master" variant="" vendor="Renesas" version="6.4.0">
      <description>I2C Master Interface</description>
      <originalPack>Renesas.RA.6.4.0.pack</originalPack>
    </component>
    <component apiversion="" class="HAL Drivers" condition="" group="all" subgroup="r_gpt" variant="" vendor="Renesas" version="6.4.0">
      <description>General PWM Timer</description>
      <originalPack>Renesas.RA.6.4.0.pack</originalPack>
    </component>
    <component apiversion="" class="HAL Drivers" condition="" group="all" subgroup="r_sci_b_uart" variant="" vendor="Renesas" version="6.4.0">
      <description>UART</description>
      <originalPack>Renesas.RA.6.4.0.pack</originalPack>
    </component>
    <component apiversion="" class="HAL Drivers" condition="" group="all" subgroup="r_usb_basic" variant="" vendor="Renesas" version="6.4.0">
      <description>USB Basic</description>
      <originalPack>Renesas.RA.6.4.0.pack</originalPack>
    </component>
    <component apiversion="" class="HAL Drivers" condition="" group="all" subgroup="r_usb_hvnd" variant="" vendor="Renesas" version="6.4.0">
      <description>USB Host Vendor Class</description>
      <originalPack>Renesas.RA.6.4.0.pack</originalPack>
    </component>
    <component apiversion="" class="Heaps" condition="" group="FreeRTOS" subgroup="heap_4" variant="" vendor="AWS" version="11.1.0+fsp.6.4.0">
      <description>FreeRTOS - Memory Management - Heap 4</description>
      <originalPack>Amazon.FreeRTOS-Kernel.11.1.0+fsp.6.4.0.pack</originalPack>
    </component>
"""
for c in ET.fromstring("<wrap>" + extra_comps + "</wrap>"):
    comp_sel.append(c)

mod_cfg = root.find("raModuleConfiguration")


def get_modules(path: Path, module_ids: list[str]):
    mc = ET.parse(path).find("raModuleConfiguration")
    return [copy.deepcopy(m) for m in mc.findall("module") if m.get("id") in module_ids]


for m in get_modules(BASE / "th_sensor/configuration.xml", ["module.driver.i2c_on_iic_master.1459752742"]):
    mod_cfg.insert(2, m)
for m in get_modules(BASE / "pump/configuration.xml", ["module.driver.timer_on_gpt.1278374793"]):
    mod_cfg.insert(3, m)

uart_xml = """
<module id="module.driver.uart_on_sci_b_uart.900000001">
  <property id="module.driver.uart.name" value="g_com_uart0"/>
  <property id="module.driver.uart.channel" value="9"/>
  <property id="module.driver.uart.data_bits" value="module.driver.uart.data_bits.data_bits_8"/>
  <property id="module.driver.uart.parity" value="module.driver.uart.parity.parity_off"/>
  <property id="module.driver.uart.stop_bits" value="module.driver.uart.stop_bits.stop_bits_1"/>
  <property id="module.driver.uart.baud" value="115200"/>
  <property id="module.driver.uart.baudrate_modulation" value="module.driver.uart.baudrate_modulation.disabled"/>
  <property id="module.driver.uart.baudrate_max_err" value="5"/>
  <property id="module.driver.uart.flow_control" value="module.driver.uart.flow_control.disabled"/>
  <property id="module.driver.uart.pin_control_port" value="module.driver.uart.pin_control_port.PORT_DISABLE"/>
  <property id="module.driver.uart.pin_control_pin" value="module.driver.uart.pin_control_pin.PIN_DISABLE"/>
  <property id="module.driver.uart.clk_src" value="module.driver.uart.clk_src.int_clk"/>
  <property id="module.driver.uart.rx_edge_start" value="module.driver.uart.rx_edge_start.falling_edge"/>
  <property id="module.driver.uart.noisecancel_en" value="module.driver.uart.noisecancel_en.disabled"/>
  <property id="module.driver.uart.rx_fifo_trigger" value="module.driver.uart.rx_fifo_trigger.max"/>
  <property id="module.driver.uart.irda.ire" value="module.driver.uart.irda.ire.disabled"/>
  <property id="module.driver.uart.irda.irrxinv" value="module.driver.uart.irda.irrxinv.disabled"/>
  <property id="module.driver.uart.irda.irtxinv" value="module.driver.uart.irtxinv.disabled"/>
  <property id="module.driver.uart.rs485.de_enable" value="module.driver.uart.rs485.de_enable.disabled"/>
  <property id="module.driver.uart.rs485.de_polarity" value="module.driver.uart.rs485.de_polarity.high"/>
  <property id="module.driver.uart.rs485.de_port_number" value="module.driver.uart.rs485.de_port_number.PORT_DISABLE"/>
  <property id="module.driver.uart.rs485.de_pin_number" value="module.driver.uart.rs485.de_pin_number.PIN_DISABLE"/>
  <property id="module.driver.uart.callback" value="UART_Rx_Callback"/>
  <property id="module.driver.uart.rxi_ipl" value="board.icu.common.irq.priority12"/>
  <property id="module.driver.uart.txi_ipl" value="board.icu.common.irq.priority12"/>
  <property id="module.driver.uart.tei_ipl" value="board.icu.common.irq.priority12"/>
  <property id="module.driver.uart.eri_ipl" value="board.icu.common.irq.priority12"/>
</module>
"""
mod_cfg.insert(5, ET.fromstring(uart_xml))

for i, m in enumerate(
    get_modules(
        BASE / "usb_light/configuration.xml",
        [
            "module.driver.hvnd_on_usb.295880289",
            "module.driver.basic_on_usb.1004914669",
        ],
    )
):
    mod_cfg.insert(6 + i, m)

heap_mod = ET.Element("module", {"id": "module.freertos.heap.4.900000002"})
freertos_port = mod_cfg.find("module[@id='module.middleware.rm_freertos_port.0']")
if freertos_port is not None:
    idx = list(mod_cfg).index(freertos_port) + 1
    mod_cfg.insert(idx, heap_mod)

adc_mod = mod_cfg.find("module[@id='module.driver.adc_on_adc_b.700000001']")
for prop in adc_mod.findall("property"):
    if prop.get("id") == "module.driver.adc.virtual_channel_1_scan_group":
        prop.set("value", "module.driver.adc.virtual_channel_1_scan_group.scan_group_0")
    if prop.get("id") == "module.driver.adc.virtual_channel_1_channel_select":
        prop.set("value", "module.driver.adc.virtual_channel_1_channel_select.an001")

hal_ctx = mod_cfg.find("context[@id='_hal.0']")
for stack in list(hal_ctx.findall("stack")):
    mod = stack.get("module", "")
    if mod not in (
        "module.driver.ioport_on_ioport.0",
        "module.middleware.rm_freertos_port.0",
    ):
        hal_ctx.remove(stack)

usb_stack = ET.Element("stack", {"module": "module.driver.hvnd_on_usb.295880289"})
ET.SubElement(
    usb_stack,
    "stack",
    {
        "module": "module.driver.basic_on_usb.1004914669",
        "requires": "module.driver.basic_on_usb.requires.basic",
    },
)
hal_ctx.append(usb_stack)

for t in list(mod_cfg.findall("context")):
    if t.get("id", "").startswith("rtos.awsfreertos.thread"):
        mod_cfg.remove(t)

threads = [
    ("rtos.awsfreertos.thread.0", "Sensor_Task", "Sensor", 12288, 2, [
        "module.driver.i2c_on_iic_master.1459752742",
        "module.driver.adc_on_adc_b.700000001",
    ]),
    ("rtos.awsfreertos.thread.1", "Actuator_Comm_Task", "ActComm", 8192, 1, [
        "module.driver.timer_on_gpt.1278374793",
        "module.driver.uart_on_sci_b_uart.900000001",
    ]),
    ("rtos.awsfreertos.thread.2", "USB_Light_Task", "USBLight", 12288, 1, []),
]
for tid, sym, name, stack, pri, stacks in threads:
    ctx = ET.Element("context", {"id": tid})
    ET.SubElement(ctx, "property", {"id": "_symbol", "value": sym})
    ET.SubElement(ctx, "property", {"id": "rtos.awsfreertos.thread.name", "value": name})
    ET.SubElement(ctx, "property", {"id": "rtos.awsfreertos.thread.stack", "value": str(stack)})
    ET.SubElement(ctx, "property", {"id": "rtos.awsfreertos.thread.priority", "value": str(pri)})
    ET.SubElement(ctx, "property", {"id": "rtos.awsfreertos.thread.context", "value": "NULL"})
    ET.SubElement(ctx, "property", {"id": "rtos.awsfreertos.thread.allocation", "value": "rtos.awsfreertos.thread.allocation.static"})
    ET.SubElement(ctx, "property", {"id": "rtos.awsfreertos.thread.secure_context", "value": "rtos.awsfreertos.thread.secure_context.enable"})
    for s in stacks:
        ET.SubElement(ctx, "stack", {"module": s})
    if tid == "rtos.awsfreertos.thread.1":
        ET.SubElement(ctx, "stack", {"module": "module.freertos.heap.4.900000002"})
    mod_cfg.append(ctx)

for prop in mod_cfg.findall("config[@id='config.awsfreertos.thread']/property"):
    if prop.get("id") == "config.awsfreertos.thread.configuse_mutexes":
        prop.set("value", "config.awsfreertos.thread.configuse_mutexes.enabled")
    if prop.get("id") == "config.awsfreertos.thread.configtotal_heap_size":
        prop.set("value", "32768")
    if prop.get("id") == "config.awsfreertos.thread.configsupport_dynamic_allocation":
        prop.set("value", "config.awsfreertos.thread.configsupport_dynamic_allocation.enabled")

for cfg_id, src_proj in [
    ("config.driver.iic_master", "th_sensor"),
    ("config.driver.gpt", "pump"),
    ("config.driver.usb_basic", "usb_light"),
]:
    if mod_cfg.find(f"config[@id='{cfg_id}']") is None:
        c = ET.parse(BASE / src_proj / "configuration.xml").find(f"raModuleConfiguration/config[@id='{cfg_id}']")
        if c is not None:
            mod_cfg.append(copy.deepcopy(c))

if mod_cfg.find("config[@id='config.driver.sci_b_uart']") is None:
    mod_cfg.append(
        ET.fromstring(
            """
<config id="config.driver.sci_b_uart">
  <property id="config.driver.sci_b_uart.param_checking_enable" value="config.driver.sci_b_uart.param_checking_enable.bsp"/>
  <property id="config.driver.sci_b_uart.fifo_support" value="config.driver.sci_b_uart.fifo_support.disabled"/>
  <property id="config.driver.sci_b_uart.dtc_support" value="config.driver.sci_b_uart.dtc_support.disabled"/>
  <property id="config.driver.sci_b_uart.flow_control" value="config.driver.sci_b_uart.flow_control.disabled"/>
</config>
"""
        )
    )

for m in mod_cfg.findall("module[@id='module.driver.ioport_on_ioport.0']/property"):
    if m.get("id") == "module.driver.ioport.pincfg":
        m.set("value", "g_bsp_pin_cfg")

pin_cfg = root.find("raPinConfiguration")
for prop_id, val in [
    ("p000.symbolic_name", "SOIL_ADC_A0"),
    ("p001.symbolic_name", "TDS_ADC_A1"),
    ("p514.symbolic_name", "THS_SDA2"),
    ("p515.symbolic_name", "THS_SCL2"),
    ("p601.symbolic_name", "UWS_DQ"),
    ("p104.symbolic_name", "PUMP_IN2"),
    ("p105.symbolic_name", "PUMP_IN1"),
    ("p206.symbolic_name", "PUMP_VREF"),
]:
    sn = ET.SubElement(pin_cfg, "symbolicName")
    sn.set("propertyId", prop_id)
    sn.set("value", val)

pincfg = pin_cfg.find("pincfg")
extra_pins = """
      <configSetting altId="gpt1.gtioc1b.p104" configurationId="gpt1.gtioc1b"/>
      <configSetting altId="gpt1.mode.gtiocaorgtiocb.free" configurationId="gpt1.mode"/>
      <configSetting altId="iic2.scl2.p515" configurationId="iic2.scl2"/>
      <configSetting altId="iic2.sda2.p514" configurationId="iic2.sda2"/>
      <configSetting altId="p104.gpt1.gtioc1b" configurationId="p104"/>
      <configSetting altId="p104.gpio_mode.gpio_mode_peripheral" configurationId="p104.gpio_mode"/>
      <configSetting altId="p105.output.low" configurationId="p105"/>
      <configSetting altId="p105.gpio_mode.gpio_mode_out.low" configurationId="p105.gpio_mode"/>
      <configSetting altId="p206.output.high" configurationId="p206"/>
      <configSetting altId="p206.gpio_mode.gpio_mode_out.high" configurationId="p206.gpio_mode"/>
      <configSetting altId="p514.gpio_mode.gpio_mode_peripheral" configurationId="p514.gpio_mode"/>
      <configSetting altId="p514.gpio_pupd.gpio_pupd_ip_up" configurationId="p514.gpio_pupd"/>
      <configSetting altId="p514.iic2.sda2" configurationId="p514"/>
      <configSetting altId="p515.gpio_mode.gpio_mode_peripheral" configurationId="p515.gpio_mode"/>
      <configSetting altId="p515.gpio_pupd.gpio_pupd_ip_up" configurationId="p515.gpio_pupd"/>
      <configSetting altId="p515.iic2.scl2" configurationId="p515"/>
      <configSetting altId="p601.output.high" configurationId="p601"/>
      <configSetting altId="p601.gpio_mode.gpio_mode_out.high" configurationId="p601.gpio_mode"/>
      <configSetting altId="sci9.mode.asynchronousuart.free" configurationId="sci9.mode"/>
      <configSetting altId="sci9.rxd9.p302" configurationId="sci9.rxd9"/>
      <configSetting altId="sci9.txd9.p301" configurationId="sci9.txd9"/>
      <configSetting altId="p301.gpio_mode.gpio_mode_peripheral" configurationId="p301.gpio_mode"/>
      <configSetting altId="p301.sci9.txd9" configurationId="p301"/>
      <configSetting altId="p302.gpio_mode.gpio_mode_peripheral" configurationId="p302.gpio_mode"/>
      <configSetting altId="p302.sci9.rxd9" configurationId="p302"/>
      <configSetting altId="p407.usbhs.usbhs_vbusen" configurationId="p407"/>
      <configSetting altId="p407.gpio_mode.gpio_mode_peripheral" configurationId="p407.gpio_mode"/>
      <configSetting altId="p408.usbhs.usbhs_vbus" configurationId="p408"/>
      <configSetting altId="p408.gpio_mode.gpio_mode_peripheral" configurationId="p408.gpio_mode"/>
      <configSetting altId="usbhs.mode.custom.free" configurationId="usbhs.mode"/>
      <configSetting altId="usbhs.usbhs_vbus.p408" configurationId="usbhs.usbhs_vbus"/>
      <configSetting altId="usbhs.usbhs_vbusen.p407" configurationId="usbhs.usbhs_vbusen"/>
"""
for el in ET.fromstring("<wrap>" + extra_pins + "</wrap>"):
    pincfg.append(el)

for prop in root.findall(".//property[@id='config.bsp.common.main']"):
    prop.set("value", "0x400")

ET.indent(root, space="  ")
with open(MERGE / "configuration.xml", "w", encoding="utf-8", newline="\n") as f:
    f.write('<?xml version="1.0" encoding="UTF-8" standalone="no"?>\n')
    f.write(ET.tostring(root, encoding="unicode"))
print("configuration.xml merged OK")

VIA = MERGE / "via" / "rasc_armclang.via"
if VIA.exists():
    text = VIA.read_text(encoding="utf-8")
    if "BSP_BOOTLOADED_APPLICATION" not in text:
        VIA.write_text(text.rstrip() + "\n-DBSP_BOOTLOADED_APPLICATION\n", encoding="utf-8")
        print("Added -DBSP_BOOTLOADED_APPLICATION to rasc_armclang.via")

SOLUTION_SCRIPT = MERGE / "scripts" / "gen_solution_xml.py"
if SOLUTION_SCRIPT.exists():
    subprocess.run([sys.executable, str(SOLUTION_SCRIPT)], check=True)
