## 下一步你需要做的事

1. 接线 —— 飞腾派 SPI0 排针 ↔ RA6E2 P100..P103，共地（< 15 cm）
2. RA6E2 工程改造 —— 严格按 `ra6e2_patch/FSP_CHANGES.md` 操作（最关键的一步是 FSP Configurator 把 SPI1 双 DMAC 字宽从 2 Byte 改为 1 Byte，否则 CRC 永远校验失败）
3. 部署到飞腾派 —— `scp build/linux/aqua_spi* user@phytium:/usr/local/bin/`
4. 联调 —— `sudo aqua_spid -f -v`，应在 5 秒内看到 `RA6E2 上线`；然后 `aqua_spi_cli sensor poll` 取一帧实测数据

如果联调时 `stats` 显示 `rx_crc_err` 持续累加，99% 是第 2 步的 DMAC 字宽没改。其他故障排查见 `README.md` 末尾。