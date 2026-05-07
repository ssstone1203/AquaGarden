# 飞腾派 Python Bridge 集成

下面把 `aqua_bridge.py` 集成为开机自启服务，供 Spring Boot `/api/aqua/*` 调用。

## 1) 同步文件到飞腾派

```bash
# 在开发机执行（把 <IP> 替换为飞腾派地址）
scp hardware/phytiumpi/aqua_bridge.py root@<IP>:/opt/aqua/bin/aqua_bridge.py
scp hardware/phytiumpi/requirements-aqua-bridge.txt root@<IP>:/opt/aqua/requirements-aqua-bridge.txt
scp hardware/phytiumpi/spi_com/deploy/systemd/aqua-bridge.service root@<IP>:/opt/aqua/spi_com/deploy/systemd/aqua-bridge.service
scp hardware/phytiumpi/spi_com/deploy/scripts/install_aqua_bridge.sh root@<IP>:/opt/aqua/spi_com/deploy/scripts/install_aqua_bridge.sh
```

## 2) 在飞腾派安装并启动

```bash
ssh root@<IP>
chmod +x /opt/aqua/spi_com/deploy/scripts/install_aqua_bridge.sh
sudo /opt/aqua/spi_com/deploy/scripts/install_aqua_bridge.sh
```

## 3) 验证

```bash
systemctl status aqua-bridge --no-pager
journalctl -u aqua-bridge -n 80 --no-pager
curl -s http://127.0.0.1:18080/api/status
```

## 4) 后端对接

Spring Boot 配置：

```properties
aquagarden.bridge.base-url=http://<飞腾派IP>:18080
```

重启后端后，前端“水泵控制”按钮即可通过后端转发到飞腾派执行 `aqua_spi_cli pump ...`。
