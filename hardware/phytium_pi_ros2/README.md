# phytium-linux-buildroot
Buildroot是一种简单、高效且易于使用的工具，可以通过交叉编译生成嵌入式Linux系统。Buildroot的用户手册位于docs/manual/manual.pdf。  
phytium-linux-buildroot基于Buildroot 2022.02，支持飞腾E2000，D2000，FT-2000/4等CPU和X100套片，编译构建飞腾Linux 4.19、5.10内核和设备树，Ubuntu20.04、22.04文件系统，Debian11文件系统，基于BusyBox文件系统等，以及整体软件镜像。  
镜像可烧录到飞腾开发板（UEFI或U-boot固件均可），用于软件开发调试。

# 预备知识
Buildroot涉及了rootfs、make、Kconfig、Linux command、Shell scripts、U-Boot、Linux kernel、dts等相关知识，了解与熟悉它们有助于Buildroot的使用。  
了解上述知识，可以参考Wiki：[相关知识与学习链接](https://gitee.com/phytium_embedded/phytium-linux-buildroot/wikis/%E7%9B%B8%E5%85%B3%E7%9F%A5%E8%AF%86%E4%B8%8E%E5%AD%A6%E4%B9%A0%E9%93%BE%E6%8E%A5)

# 开发环境
## 系统要求
我们只支持在Ubuntu20.04、Ubuntu22.04、Debian11这三种主机上运行phytium-linux-buildroot，不支持其他系统。
Buildroot需要主机系统上安装以下依赖包：
```
$ sudo apt update
$ sudo apt install debianutils sed make binutils build-essential gcc \
g++ bash patch gzip bzip2 perl tar cpio unzip rsync file bc wget git libncurses5-dev \
debootstrap qemu-user-static binfmt-support debian-archive-keyring
```
对于Debian11主机系统，还需要：  
（1）设置PATH环境变量：`PATH=$PATH:/usr/sbin`  
（2）从Debian11 backports源安装debootstrap 1.0.128+nmu2+deb12u1~bpo11+1版本：  
确保在/etc/apt/sources.list中添加了Debian11 backports源，例如`deb http://ftp.cn.debian.org/debian bullseye-backports main`，
然后运行
```
$ sudo apt update
$ sudo apt install -t bullseye-backports debootstrap
```

## 下载phytium-linux-buildroot
`$ git clone https://gitee.com/phytium_embedded/phytium-linux-buildroot.git`

# 默认配置文件和扩展配置文件
为飞腾CPU平台构建的文件系统的配置文件位于configs目录。  
其中，以defconfig结尾的为默认配置文件，以config结尾的为扩展配置文件。  
defconfig可以和config文件进行组合，用以扩展特定的功能，但要注意不能随意组合。  
## defconfig
在phytium-linux-buildroot根目录下执行`$ make list-defconfigs`，返回configs目录中的defconfig配置文件。  
```
$ cd xxx/phytium-linux-buildroot
$ make list-defconfigs
```
其中以phytium开头的为飞腾相关的defconfig配置文件，包含：  
```
phytium_debian_defconfig      - Build for debian system
phytium_defconfig             - Build for busybox minimal system
phytium_ubuntu_defconfig      - Build for ubuntu system
```
## config
config文件是功能扩展配置文件，具体的文件及功能如下：
| config | 功能 |
|------------------------|--------|
| linux_xxx.config | 内核配置文件，含有linux 4.19、4.19 rt和5.10 rt内核版本 |
| ubuntu_20.04.config | Ubuntu20.04 |
| desktop.config | XFCE桌面 |
| desktop_gnome.config | GNOME桌面 |
| e2000_optee.config | Phytium-optee |
| xenomai_xxx.config | cobalt_4.19、cobalt_5.10、mercury_4.19和mercury_5.10 |
| ethercat.config | ethercat |
| jailhouse.config | jailhouse |
| x100.config | X100 |
| qt5_eglfs.config | 无桌面qt5 eglfs，需要x100.config的支持 |
| openamp_xxx.config | OpenAMP |
| initramfs.config | initramfs |
| ros2.config | ros2 |
| phytiumpi_sdcard.config | 飞腾派配置文件 |
| kernel_debug.config | 内核debug配置文件 |
| host_arm64.config | 支持编译主机为arm64主机 |
| toolchain_buildroot.config | 支持buildroot使用内部工具链 |
| amdgpu.config | 支持AMD显卡 |
| weston.config | 支持简单的weston桌面 |
## 配置文件的组合
defconfig可以和config文件进行组合（一个defconfig可以和多个config文件组合），用以扩展特定的功能，defconfig与config的组合关系如下：
| deconfig | config |
|------------------------|--------|
| phytium_ubuntu_defconfig | linux_xxx.config、ubuntu_20.04.config、desktop.config、desktop_gnome.config、e2000_optee.config、xenomai_xxx.config、ethercat.config、jailhouse.config、x100.config、qt5_eglfs.config、openamp_xxx.config、initramfs.config、ros2.config、phytiumpi_sdcard.config、kernel_debug.config、host_arm64.config、toolchain_buildroot.config、amdgpu.config、weston.config  |
| phytium_debian_defconfig | linux_xxx.config、desktop.config、e2000_optee.config、xenomai_xxx.config、ethercat.config、jailhouse.config、x100.config、qt5_eglfs.config、openamp_xxx.config、initramfs.config、phytiumpi_sdcard.config 、host_arm64.config、toolchain_buildroot.config、amdgpu.config、weston.config |
| phytium_defconfig | linux_xxx.config、e2000_optee.config、phytiumpi_sdcard.config、xenomai_xxx.config、ethercat.config、jailhouse.config、openamp_xxx.config 、host_arm64.config、toolchain_buildroot.config、amdgpu.config |

# 编译文件系统
## 编译默认配置的文件系统
（1）加载defconfig   
`$ make phytium_xxx_defconfig`  
其中`phytium_xxx_defconfig`为以下文件系统之一：
```
phytium_ubuntu_defconfig
phytium_debian_defconfig
phytium_defconfig
```  
（2）编译  
`$ make`  
（3）镜像的输出位置  
生成的根文件系统、内核、img 镜像位于output/images目录。 

### UEFI启动基于BusyBox的最小系统
UEFI启动时，UART1串口对应的设备文件是ttyAMA0，所以如果要编译基于BusyBox的最小系统，需要将`phytium-linux-buildroot/board/phytium/common/busybox_init_overlay/etc/inittab`中的
`#ttyAMA0::respawn:/sbin/getty -L ttyAMA0 115200 vt100`一行，取消注释，再执行编译。

### cpio文件
将基于busybox的系统，进行cpio打包和压缩生成了rootfs.cpio.gz，使用bootloader可以把它加载到内存中，从内存启动这个根文件系统。  

### img 镜像
目前buildroot支持编译img 镜像（disk.img），生成的img 镜像位于output/images目录。img 镜像包含了根文件系统、内核、设备树和GRUB。
使用img 镜像安装系统，不需要像之前那样将存储设备手动分区再拷贝文件，只需要将disk.img文件写入存储设备即可。  

## 支持的功能
### 更换文件系统的linux内核版本
defconfig中的内核版本默认是linux 5.10。我们支持在编译文件系统时将内核版本更换为linux 4.19，linux 4.19 rt，linux 5.10 rt。
关于linux内核的信息请参考：`https://gitee.com/phytium_embedded/phytium-linux-kernel`  
更换内核版本的操作步骤为：  
（1）使用phytium_debian_defconfig、phytium_ubuntu_defconfig或phytium_defconfig作为基础配置项，合并支持其他内核版本的配置：  
`$ ./support/kconfig/merge_config.sh configs/phytium_xxx_defconfig configs/linux_xxx.config`  
其中，`configs/linux_xxx.config`为以下配置片段文件之一：
```
configs/linux_4.19.config
configs/linux_4.19_rt.config
configs/linux_5.10_rt.config
```
这三个文件分别对应于linux 4.19，linux 4.19 rt，linux 5.10 rt内核。  
（2）编译  
`$ make`  
（3）镜像的输出位置  
生成的根文件系统、内核、img 镜像位于output/images目录。

### 更换Ubuntu版本
使用phytium_ubuntu_defconfig编译的Ubuntu系统，默认版本是Ubuntu22.04，如果需要编译Ubuntu20.04请执行：  
（1）使用phytium_ubuntu_defconfig作为基础配置项，合并支持Ubuntu20.04的配置：  
`$ ./support/kconfig/merge_config.sh configs/phytium_ubuntu_defconfig configs/ubuntu_20.04.config`  
（2）编译  
`$ make`  
（3）镜像的输出位置  
生成的根文件系统、内核、img 镜像位于output/images目录。

#### Firefox浏览器（Ubuntu22.04）
Ubuntu22.04提供的是snap版的firefox，系统启动后，需要运行`sudo snap install firefox`命令安装firefox snap软件包，
然后点击浏览器图标打开firefox。  
注意：  
（1）如果是通过手动分区的方式安装系统，将根文件系统解压到分区的命令应该为：  
```
sudo tar --xattrs --xattrs-include='*' -xf rootfs.tar
```
从而可以保留文件的扩展属性。  
但是如果使用tar解压根文件系统时，没有指定`--xattrs --xattrs-include='*'`参数，那么安装firefox后会出现打不开浏览器的问题，
此时在终端执行`firefox`命令会看到如下错误，且`getcap /usr/lib/snapd/snap-confine`返回空。
```
user@phytium-Ubuntu:~$ firefox
snap-confine is packaged without necessary permissions and cannot continue
required permitted capability cap_dac_override not found in current capabilities:
  =: Function not implemented
user@phytium-Ubuntu:~$ getcap /usr/lib/snapd/snap-confine
user@phytium-Ubuntu:~$ 
```
此时需要重新配置snapd，再重新打开firefox即可：
```
user@phytium-Ubuntu:~$ sudo dpkg-reconfigure snapd
```
如果是使用img 镜像（disk.img）安装系统，则无此问题。  

（2）我们不建议Ubuntu22.04和linux 4.19内核组合使用，此时snap应用程序无法正常工作。  
如果确实需要在Ubuntu22.04 + 4.19内核环境上跑snap版本的firefox，需要进行以下操作：  
- 内核选项打开CONFIG_BPF_SYSCALL=y，CONFIG_CGROUP_BPF=y，以及CONFIG_CGROUP_FREEZER=y。  
- 修改启动参数，在linux内核命令行参数里加上systemd.unified_cgroup_hierarchy=0，切换到cgroup v1启动。

### 同时更换Linux内核版本和Ubuntu版本
比如，需要编译Linux4.19内核的Ubuntu20.04请执行：  
（1）使用phytium_ubuntu_defconfig作为基础配置项，合并支持Linux-4.19内核、Ubuntu20.04的配置：  
`$ ./support/kconfig/merge_config.sh configs/phytium_ubuntu_defconfig configs/linux_4.19.config configs/ubuntu_20.04.config`  
（2）编译  
`$ make`  
（3）镜像的输出位置  
生成的根文件系统、内核、img 镜像位于output/images目录。

### 支持XFCE桌面
（1）使用phytium_debian_defconfig或phytium_ubuntu_defconfig作为基础配置项，合并支持desktop的配置：  
`$ ./support/kconfig/merge_config.sh configs/phytium_xxx_defconfig configs/desktop.config`  
（2）编译  
`$ make`  
（3）镜像的输出位置  
生成的根文件系统、内核位于output/images 目录。  

### 支持GNOME桌面
（1）使用phytium_debian_defconfig或phytium_ubuntu_defconfig作为基础配置项，合并支持desktop_gnome的配置：  
`$ ./support/kconfig/merge_config.sh configs/phytium_xxx_defconfig configs/desktop_gnome.config`  
（2）编译  
`$ make`  
（3）镜像的输出位置  
生成的根文件系统、内核位于output/images 目录。  

### 支持Phytium-optee
Phytium-optee只支持E2000开发板，关于Phytium-optee的信息请参考：`https://gitee.com/phytium_embedded/phytium-optee`  
defconfig默认不编译Phytium-optee，如果需要编译Phytium-optee请执行：  
（1）使用phytium_debian_defconfig、phytium_ubuntu_defconfig或phytium_defconfig作为基础配置项，合并支持optee的配置：  
`$ ./support/kconfig/merge_config.sh configs/phytium_xxx_defconfig configs/e2000_optee.config`  
目前Phytium-optee支持的E2000开发板有E2000D DEMO和E2000Q DEMO。  
（2）编译  
`$ make`  
（3）镜像的输出位置  
生成的根文件系统、内核、TEE OS位于output/images目录。  
后续部署及使用方法，请参考`https://gitee.com/phytium_embedded/phytium-embedded-docs/tree/master/optee`

### 支持xenomai
xenomai支持E2000和D2000开发板，关于xenomai的信息请参考：`https://gitee.com/phytium_embedded/linux-kernel-xenomai`  
支持将xenomai内核及用户态的库、工具编译安装到系统上。如果需要编译xenomai请执行：  
（1）使用phytium_debian_defconfig、phytium_ubuntu_defconfig或phytium_defconfig作为基础配置项，合并支持xenomai的配置：  
`$ ./support/kconfig/merge_config.sh configs/phytium_xxx_defconfig configs/xenomai_xxx.config`  
其中，`xenomai_xxx.config`为以下配置片段文件之一：
```
xenomai_cobalt_4.19.config  （xenomai cobalt 4.19内核+xenomai-v3.1.5.tar.gz）
xenomai_cobalt_5.10.config  （xenomai cobalt 5.10内核+xenomai-v3.2.6.tar.gz）
xenomai_mercury_4.19.config （linux 4.19 rt内核+xenomai-v3.1.5.tar.gz）
xenomai_mercury_5.10.config （linux 5.10 rt内核+xenomai-v3.2.6.tar.gz）
```
（2）编译  
`$ make`  
（3）镜像的输出位置  
生成的根文件系统、内核、img 镜像位于output/images目录。  
（4）文件的安装路径  
xenomai用户态的库、工具被安装到根文件系统的/usr/xenomai目录。  
关于xenomai的启动及测试工具等更多信息，请参考`https://gitee.com/phytium_embedded/phytium-embedded-docs/tree/master/linux/xenomai`   

### 支持ethercat
ethercat支持E2000开发板，关于ethercat的信息请参考：`https://gitee.com/phytium_embedded/ether-cat`  
支持将ethercat驱动及用户态的库、工具编译安装到系统上，ethercat支持linux 4.19 rt，linux 5.10 rt，xenomai_cobalt_4.19，xenomai_cobalt_5.10内核。如果需要编译ethercat请执行：  
（1）使用phytium_debian_defconfig、phytium_ubuntu_defconfig或phytium_defconfig作为基础配置项，合并支持rt内核，及ethercat的配置：  
`./support/kconfig/merge_config.sh configs/phytium_xxx_defconfig configs/linux_xxx_rt.config configs/ethercat.config`  
其中，`linux_xxx_rt.config`为`linux_4.19_rt.config`或`linux_5.10_rt.config`。  
或者合并支持xenomai内核：  
`./support/kconfig/merge_config.sh configs/phytium_xxx_defconfig configs/xenomai_cobalt_xxx.config configs/ethercat.config`  
其中，`xenomai_cobalt_xxx.config`为`xenomai_cobalt_4.19.config`或`xenomai_cobalt_5.10.config`。  
（2）编译  
`$ make`  
（3）镜像的输出位置  
生成的根文件系统、内核、img 镜像位于output/images目录。  
（4）文件的安装路径  
将ethercat的驱动模块安装到根文件系统的/lib/modules/version/ethercat/目录，并且通过将ec_macb加入/etc/modprobe.d/blacklist.conf
黑名单的方式，使得开机时不自动加载ec_macb模块，而是让用户手动加载。  
ethercat用户态的库、工具被安装到根文件系统：  
配置文件安装到/etc，其它内容分别被安装到/usr目录下的bin，include，lib，sbin，share。  
关于ethercat的使用方法等更多信息，请参考`https://gitee.com/phytium_embedded/phytium-embedded-docs/tree/master/linux/ethercat`  

### 支持jailhouse
jailhouse支持E2000、D2000和FT-2000/4、D3000、D3000M开发板，关于jailhouse的信息请参考：`https://gitee.com/phytium_embedded/phytium-jailhouse`  
支持将jailhouse编译安装到系统上，如果需要编译jailhouse请执行：  
（1）使用phytium_debian_defconfig、phytium_ubuntu_defconfig或phytium_defconfig作为基础配置项，合并支持jailhouse的配置：  
`./support/kconfig/merge_config.sh configs/phytium_xxx_defconfig configs/jailhouse.config`    
（2）编译  
`$ make`  
（3）镜像的输出位置  
生成的根文件系统、内核、img 镜像位于output/images目录。  
（4）文件的安装路径  
jailhouse相关的文件被安装到根文件系统：  
```
按照Makefile中的规则，
将jailhouse的驱动jailhouse.ko安装到/lib/modules/version/jailhouse/driver；
jailhouse.bin安装到/lib/firmware；
linux-loader.bin安装到/usr/libexec/jailhouse；
jailhouse和ivshmem-demo安装到/usr/sbin；
python helper脚本安装到/usr/libexec/jailhouse；
jailhouse-config-collect.tmpl和root-cell-config.c.tmpl安装到/usr/share/jailhouse；
jailhouse-completion.bash安装到/usr/share/bash-completion/completions/并改名为jailhouse；
另外，还将configs/*/*.cell安装到/etc/jailhouse；
inmates/demos/*/*.bin安装到/usr/libexec/jailhouse/demos；
configs/arm64/dts/*.dtb安装到/usr/libexec/jailhouse/dtb；
pyjailhouse安装到/usr/lib/python3.10/site-packages，
通过/usr/lib/python3/dist-packages/pyjailhouse.pth文件，将pyjailhouse模块添加到python模块的搜索路径。
```
关于jailhouse的使用方法等更多信息，请参考`https://gitee.com/phytium_embedded/phytium-jailhouse/blob/master/Readme.md`  

### 支持X100
在Ubuntu和Debian系统中安装了linux-headers和dkms，从而支持使用dkms来构建内核模块。  
在配置及编译文件系统时，需要合并x100.config：  
```
$ ./support/kconfig/merge_config.sh configs/phytium_xxx_defconfig configs/desktop.config configs/x100.config
$ make
```
系统启动后，可以使用dpkg命令安装X100的deb包。  

### 支持AMD显卡
编译Ubuntu和Debian系统时，支持合并amdgpu.config配置项，以达到支持AMD显卡的目的。为避免显示冲突，启用该配置项时，会禁止DC显示和VPU功能。  
如果需要支持AMD显卡请执行：  
```
$ ./support/kconfig/merge_config.sh configs/phytium_xxx_defconfig configs/desktop.config configs/amdgpu.config
$ make
```
AMD显卡对应的固件放在了/lib/firmware/amdgpu和/lib/firmware/radeon目录下。  

### 支持无桌面qt5 eglfs
支持将qt5 eglfs编译安装到无桌面的Ubuntu20.04、Ubuntu22.04和Debian11系统，用来运行在带x100的开发板上。  
如果需要编译qt5 eglfs请执行：  
```
$ ./support/kconfig/merge_config.sh configs/phytium_xxx_defconfig configs/qt5_eglfs.config configs/x100.config
$ make
```
qt5相关的文件被安装到根文件系统的/usr目录：
头文件安装到/usr/include/qt5，库安装到/usr/lib，plugins安装到/usr/lib/qt/plugins，examples安装到/usr/lib/qt/examples。  
系统启动后，可以使用dpkg命令安装X100的deb包。

### 支持OpenAMP
本项目还支持编译OpenAMP，手动编译OpenAMP裸跑二进制镜像请参考：`https://gitee.com/phytium_embedded/phytium-standalone-sdk` ，
手动编译OpenAMP FreeRTOS二进制镜像请参考：`https://gitee.com/phytium_embedded/phytium-free-rtos-sdk`  
Buildroot中集成了OpenAMP裸跑二进制镜像和OpenAMP FreeRTOS二进制镜像，支持将OpenAMP二进制镜像、用户空间测试程序编译并安装到系统上，OpenAMP支持linux 5.10内核。如果需要编译OpenAMP请执行：   
（1）根据自己开发板的CPU型号，修改configs/openamp_standalone.config中的变量BR2_PACKAGE_PHYTIUM_STANDALONE_CPU_NAME，
以及configs/openamp_free_rtos.config中的变量BR2_PACKAGE_PHYTIUM_FREE_RTOS_CPU_NAME，支持的值有”pe2204”, “phytiumpi”, “pd2008”, “pd1904”, “pd2308”, “pd2408”。  
（2）使用phytium_debian_defconfig、phytium_ubuntu_defconfig或phytium_defconfig作为基础配置项，合并支持openamp的配置：  
`./support/kconfig/merge_config.sh configs/phytium_xxx_defconfig configs/openamp_xxx.config`   
其中，`openamp_xxx.config`为`openamp_standalone.config`或`openamp_free_rtos.config`。  
（3）编译  
`$ make`  
（4）镜像的输出位置  
生成的根文件系统、内核、img 镜像位于output/images目录。  
（5）文件的安装路径  
将OpenAMP二进制镜像openamp_core0.elf安装到/lib/firmware；用户空间测试程序安装到/usr/bin。  
关于OpenAMP的使用方法等更多信息，请参考`https://gitee.com/phytium_embedded/phytium-embedded-docs/tree/master/open-amp`  

### 支持kernel_debug
支持将kernel_debug的一些工具编译安装到Ubuntu22系统。kernel_debug工具包含Dynamic debug、FTRACE、KPROBE和BTF等，该kernel_debug当前只支持5.10内核，具体信息请参考：[一些kernel debug config和相关用法](https://gitee.com/phytium_embedded/phytium-linux-kernel/wikis/%E8%B0%83%E8%AF%95%E4%B8%8E%E8%B0%83%E4%BC%98/%E4%B8%80%E4%BA%9Bkernel%20debug%20config%E5%92%8C%E7%9B%B8%E5%85%B3%E7%94%A8%E6%B3%95)   
如果需要编译kernel_debug请执行：  
```
$ ./support/kconfig/merge_config.sh configs/phytium_ubuntu_defconfig configs/kernel_debug.config
$ make
```

### initramfs
支持为Ubuntu和Debian系统生成initramfs，如果需要使用initramfs，请按照以下步骤编译：  
（1）使用默认的configs/initramfs.config进行配置，生成的initramfs是不支持dropbear ssh服务的，因为这会在启动initramfs的过程中执行dropbear
以及配置网络，增加了启动时间。  
如果确实需要通过ssh登录initramfs，需要修改configs/initramfs.config，增加：
```
BR2_ROOTFS_INITRAMFS_SSH=y
BR2_ROOTFS_INITRAMFS_SSH_KEY="/home/xxx/id_rsa.pub"
其中，BR2_ROOTFS_INITRAMFS_SSH_KEY的值是ssh客户端公钥的路径。
```
（2）使用phytium_ubuntu_defconfig或phytium_debian_defconfig作为基础配置项，合并支持initramfs的配置：   
`$ ./support/kconfig/merge_config.sh configs/phytium_xxx_defconfig configs/desktop.config configs/initramfs.config`  
（3）编译  
`$ make`  
（4）镜像的输出位置  
生成的initramfs（initrd.img）、根文件系统、内核、img 镜像位于output/images目录。 
此时将initrd.img包含在disk.img镜像的第一个分区，且在/EFI/BOOT/grub.cfg中添加了`initrd /initrd.img`命令。  
关于initramfs的使用，参考本仓库的Wiki。

### ROS2
支持在Ubuntu22桌面系统上安装ROS2，不支持Ubuntu20和Debian11系统，如果需要安装ROS2，请按照以下步骤编译：  
（1）使用phytium_ubuntu_defconfig作为基础配置项，合并支持desktop和ROS2的配置：   
`$ ./support/kconfig/merge_config.sh configs/phytium_ubuntu_defconfig configs/desktop.config configs/ros2.config`  
（2）编译  
`$ make`  
（3）镜像的输出位置  
生成的根文件系统、内核、img 镜像位于output/images目录。  
后续使用方法请参考《飞腾嵌入式Ubuntu系统ROS2软件使用手册》，位于`https://gitee.com/phytium_embedded/phytium-embedded-docs/tree/master/linux` 目录。

### 支持为phytiumpi编译SD卡镜像
飞腾派开发板默认只能通过SD卡启动，如果需要编译飞腾派SD卡镜像，请按照以下步骤编译：  
（1）使用phytium_debian_defconfig、phytium_ubuntu_defconfig或phytium_defconfig作为基础配置项，合并支持phytiumpi_sdcard的配置：  
`$ ./support/kconfig/merge_config.sh configs/phytium_xxx_defconfig configs/phytiumpi_sdcard.config`  
（2）编译  
`$ make`  
（3）镜像的输出位置  
生成的根文件系统、内核位于 output/images 目录。 sdcard.img 就是 SD 的镜像文件。  
后续部署及使用方法，请参考`https://gitee.com/phytium_embedded/phytium-embedded-docs/tree/master/phytiumpi/linux`  

### 支持host_arm64
Buildroot默认采用x86机器作为编译主机，如果需要支持arm64机器作为编译主机，请执行：  
（1）使用phytium_xxx_defconfig作为基础配置项，合并支持host_arm64的配置：  
`$ ./support/kconfig/merge_config.sh configs/phytium_xxx_defconfig configs/host_arm64.config`  
（2）编译  
`$ make`  

### 支持内部工具链
phytium-linux-buildroot默认使用外部工具链（预先构建好的交叉编译工具链），如果需要使用内部工具链（Buildroot从源码构建交叉编译工具链），请执行：  
（1）使用phytium_xxx_defconfig作为基础配置项，合并支持toolchain_buildroot的配置：  
`$ ./support/kconfig/merge_config.sh configs/phytium_xxx_defconfig configs/toolchain_buildroot.config`  
（2）编译  
`$ make`  

### 支持weston+qlauncher
Wayland是一个用于替代X Window System（通常简称为X）的计算机显示服务器通信协议，旨在提供更现代、简化和高性能的图形用户界面（GUI）体验。  
Weston作为Wayland协议的一个示例实现，主要目的是作为Wayland协议的参考实现，但它的设计允许它运行在不同的图形环境中，包括传统的X11环境。  
weston.config实现了weston+qlauncher的简单桌面，显示服务默认使用Weston drm后端，并提供了一个简单的qt launcher应用。  
如果需要使用weston+qlauncher功能，请使用不带桌面的配置项phytium_xxx_defconfig，合并weston.config配置项：  
`./support/kconfig/merge_config.sh configs/phytium_xxx_defconfig configs/weston.config`  
其中，phytium_xxx_defconfig可以是phytium_debian_defconfig或者phytium_ubuntu_defconfig。  
（2）编译  
`$ make`  
（3）镜像的输出位置  
生成的根文件系统、内核、sdcard.img 镜像位于output/images目录。  

## 清理编译结果
（1）`$ make clean`  
删除所有编译结果，包括output目录下的所有内容。当编译完一个文件系统后，编译另一个文件系统前，需要执行此命令。  
（2）`$ make distclean`  
重置buildroot，删除所有编译结果、下载目录以及配置。  

## 树外构建
默认情况下，buildroot生成的所有内容都存储在buildroot源码目录的output目录。然而，buildroot也支持树外构建。树外构建允许使用除output以外的其他输出目录。
使用树外构建时，buildroot的.config和临时文件也存储在输出目录中。这意味着使用相同的buildroot源码树，只要使用不同的输出目录，可以并行运行多个构建。  
使用树外构建进行配置的方式有以下三种：  
（1）在buildroot源码目录运行，使用O变量指定输出目录：  
`$ make O=xxx/foo-output phytium_xxx_defconfig`  
（2）在一个空的输出目录运行，需要指定O变量和buildroot源码树的路径：  
`$ mkdir xxx/foo-output`  
`$ cd xxx/foo-output`  
`$ make -C xxx/phytium-linux-buildroot/ O=$(pwd) phytium_xxx_defconfig`  
（3）对于使用`merge_config.sh`合并配置文件的情况，在buildroot源码目录运行：  
`$ mkdir xxx/foo-output`  
`$ ./support/kconfig/merge_config.sh -O xxx/foo-output configs/phytium_xxx_defconfig configs/xxx.config`  

运行上述命令之一，buildroot会在输出目录中创建一个Makefile，所以在输出目录中再次运行make时，不再需要指定O变量和buildroot源码树的路径。  
因此配置完成后，编译的命令为：  
`$ cd xxx/foo-output`  
`$ make`

## buildroot中修改内核进行编译
buildroot中编译内核源码的目录是`output/build/linux-<version>`，如果在该目录对内核进行了修改（例如修改内核配置或源码），
当运行`make clean`后该目录会被删除，所以在该目录中直接修改内核是不合适的。  
因此，buildroot对于这种情况提供了一种机制：`<PKG>_OVERRIDE_SRCDIR`机制。  
操作方法是，创建一个叫做local.mk的文件，其内容是：
```
$ cat local.mk 
LINUX_OVERRIDE_SRCDIR = /home/xxx/linux-kernel
```
将local.mk文件和buildroot的.config文件放在同一个目录下，对于树内构建是顶层的buildroot源码目录，对于树外构建是树外构建的输出目录。  
LINUX_OVERRIDE_SRCDIR指定了一个本地的内核源码目录，这样buildroot就不会去下载、解压、打补丁内核源码了，而是使用LINUX_OVERRIDE_SRCDIR
指定的内核源码目录。   
这样开发人员首先在LINUX_OVERRIDE_SRCDIR指定的目录对内核进行修改，然后运行`make linux-rebuild`或`make linux-reconfigure`即可。
该命令首先将LINUX_OVERRIDE_SRCDIR中的内核源码同步到`output/build/linux-custom`目录，然后进行配置、编译、安装。  
如果想要编译、安装内核，并重新生成系统镜像，请运行`make linux-rebuild all`；若是编译phytiumpi，请运行'make linux-rebuild phyuboot-rebuild all'。  

# 在开发板上启动系统
## 安装系统
可以通过手动分区的方式安装系统，将存储设备分区、格式化后，再将buildroot生成的文件拷贝到对应的分区；
也可以使用img 镜像安装系统，这种方式不需要手动将存储设备分区再拷贝文件，只需要将disk.img文件写入存储设备即可。  
### 手动分区安装系统
（1）主机端将存储设备分成两个分区（以主机识别设备名为/dev/sdb 为例，请按实际识别设备名更改）  
`$ sudo fdisk /dev/sdb`  
这将启动fdisk程序，在其中输入命令：
```
输入g创建GPT分区表；
输入n创建分区，需要创建两个分区；
输入t将第一个分区的分区类型修改为'EFI System'；
输入p打印分区表，确保输出中包含'Disklabel type: gpt'及第一个分区的类型为'EFI System'；
输入w将分区表写入磁盘并退出。
```
（2）格式化分区  
```
$ sudo mkfs.vfat /dev/sdb1
$ sudo mkfs.ext4 /dev/sdb2
```
（3）将内核、设备树和GRUB拷贝到第一个分区（EFI分区），将根文件系统拷贝到第二个分区（根分区）
```
$ sudo mount /dev/sdb1 /mnt
$ sudo cp xxx/phytium-linux-buildroot/output/images/Image /mnt
$ sudo cp xxx/phytium-linux-buildroot/output/images/e2000q-demo-board.dtb /mnt（用于U-Boot启动）
$ sudo cp -r xxx/phytium-linux-buildroot/output/images/efi-part/EFI/ /mnt（用于UEFI启动）
$ sync
$ sudo umount /dev/sdb1
$ sudo mount /dev/sdb2 /mnt
$ sudo cp xxx/phytium-linux-buildroot/output/images/rootfs.tar /mnt
$ cd /mnt
$ sudo tar --xattrs --xattrs-include='*' -xf rootfs.tar
$ sync
$ cd ~
$ sudo umount /dev/sdb2
```
### 使用img 镜像安装系统
将img 镜像（disk.img）写入存储设备：  
`$ sudo dd if=xxx/phytium-linux-buildroot/output/images/disk.img of=/dev/sdb bs=1M`  
`$ sync`  

## 启动系统
### U-Boot启动系统
安装系统后，将存储设备接到开发板，启动开发板电源，串口输出U-Boot命令行，设置U-Boot环境变量来启动系统。  
SATA盘：  
```
=>setenv bootargs console=ttyAMA1,115200  audit=0 earlycon=pl011,0x2800d000 root=/dev/sda2 rw cma=256M;
=>saveenv;
=>fatload scsi 0:1 0x90100000 Image;
=>fatload scsi 0:1 0x90000000 e2000q-demo-board.dtb;
=>booti 0x90100000 - 0x90000000
```
U盘：
```
=>setenv bootargs console=ttyAMA1,115200  audit=0 earlycon=pl011,0x2800d000 root=/dev/sda2 rootdelay=5 rw cma=256M;
=>saveenv;
=>usb start
=>fatload usb 0:1 0x90100000 Image;
=>fatload usb 0:1 0x90000000 e2000q-demo-board.dtb;
=>booti 0x90100000 - 0x90000000
```
nvme盘：
```
=>setenv bootargs console=ttyAMA1,115200  audit=0 earlycon=pl011,0x2800d000 root=/dev/nvme0n1p2 rootdelay=5 rw cma=256M;
=>saveenv;
=>fatload nvme 0:1 0x90100000 Image;
=>fatload nvme 0:1 0x90000000 e2000q-demo-board.dtb;
=>booti 0x90100000 - 0x90000000
```
注：由于执行了saveenv命令，下次启动时不用执行第一条setenv bootargs命令。

### UEFI启动系统
如果是使用img 镜像安装系统，将存储设备接到开发板，启动开发板电源，即可自动启动系统。  
如果是通过手动分区的方式安装系统，安装系统后，还需要在主机端配置存储设备的EFI分区中的/EFI/BOOT/grub.cfg文件，确保菜单条目中内核命令行参数root的值为根分区（这里是第二个分区）。  
（1）通过分区UUID  
获取第二个分区的UUID：  
`$ sudo blkid -s PARTUUID -o value /dev/sdb2`  
将grub.cfg中root的值设置为"root=PARTUUID=xxx"，xxx为分区UUID的值。  
（2）通过设备名  
将grub.cfg中root的值设置为"root=/dev/sda2"。  
然后将存储设备接到开发板，启动开发板电源，自动启动系统。  

注意：如果使用UEFI中的ACPI表或设备树文件启动有问题，可以使用img 镜像中的设备树，使用方法参考Wiki：
[UEFI+DTB启动系统](https://gitee.com/phytium_embedded/phytium-linux-buildroot/wikis/UEFI%2BDTB%E5%90%AF%E5%8A%A8%E7%B3%BB%E7%BB%9F)

## 登录  
Ubuntu和Debian系统包含了超级用户root，和一个普通用户user，超级用户和普通用户的密码都是Phytium@123，busybox最小系统只需要输入用户名root。  

# 编译内核模块
关于如何编译内核外部模块，可参考https://www.kernel.org/doc/html/latest/kbuild/modules.html   

## 交叉编译内核模块
使用工具链来交叉编译内核模块，工具链位于`output/host/bin`，工具链的sysroot为
`output/host/aarch64-buildroot-linux-gnu/sysroot`。  
交叉编译内核外部模块的命令为：
```
$ make ARCH=arm64 \
CROSS_COMPILE=/home/xxx/phytium-linux-buildroot/output/host/bin/aarch64-none-linux-gnu- \
-C /home/xxx/phytium-linux-buildroot/output/build/linux-xxx/ \
M=$PWD \
modules
```

## 开发板上编译内核模块
利用linux-headers可以在开发板上进行内核模块编译，软链接`/lib/modules/xxx/build`指向linux-headers。  
在开发板上编译内核外部模块的命令为：  
`make -C /lib/modules/xxx/build M=$PWD modules`

# buildroot编译新的应用软件
本节简单介绍如何通过buildroot交叉编译能运行在开发板上的应用软件，完整的教程请参考buildroot用户手册manual.pdf。  
## buildroot软件包介绍
buildroot中所有用户态的软件包都在package目录，每个软件包有自己的目录`package/<pkg>`，其中`<pkg>`是小写的软件包名。这个目录包含：  
（1）`Config.in`文件，用Kconfig语言编写，描述了包的配置选项。  
（2）`<pkg>.mk`文件，用make编写，描述了包如何构建，即从哪里获取源码，如何编译和安装等。  
（3）`<pkg>.hash`文件，提供hash值，检查下载文件的完整性，如检查下载的软件包源码是否完整，这个文件是可选的。  
（4）`*.patch`文件，在编译之前应用于源码的补丁文件，这个文件是可选的。  
（5）可能对包有用的其他文件。
## 编写buildroot软件包
首先创建软件包的目录`package/<pkg>`，然后编写该软件包中的文件。  
buildroot中的软件包基本上由`Config.in`和`<pkg>.mk`两个文件组成。关于如何编写这两个文件，可以参考buildroot用户手册，这里简单概括一下。  
（1）`Config.in`文件中必须包含启用或禁用该包的选项，而且必须命名为`BR2_PACKAGE_<PKG>`，其中`<PKG>`是大写的软件包名，这个选项的值是布尔类型。
也可以定义其他功能选项来进一步配置该软件包。然后还必须在`package/Config.in`文件中包含该文件：  
`source "package/<pkg>/Config.in"`  
（2）`<pkg>.mk`文件看起来不像普通的Makefile文件，而是一连串的变量定义，而且必须以大写的包名作为变量的前缀。最后以调用软件包的基础设施（package
infrastructure）结束。变量告诉软件包的基础设施要做什么。  
对于使用手写Makefile来编译的软件源码，在`<pkg>.mk`中调用generic-package基础设施。generic-package基础设施实现了包的下载、提取、打补丁。
而配置、编译和安装由`<pkg>.mk`文件描述。`<pkg>.mk`文件中可以设置的变量及其含义，请参考buildroot用户手册。  
## 编译软件包 
（1）单独编译软件包
```
$ cd xxx/phytium-linux-buildroot
$ make <pkg>
```
编译结果在`output/build/<pkg>-<version>`  

（2）将软件包编译进根文件系统
```
在phytium_xxx_defconfig中添加一行BR2_PACKAGE_<PKG>=y
$ make phytium_xxx_defconfig
$ make
```

# FAQ
1. 下载Ubuntu及Debian太慢或报错？  
目前下载Ubuntu及Debian的源为清华大学镜像，如果遇到下载很慢，或者下载报错：
`E: Unable to fetch some archives, maybe run apt-get update or try with --fix-missing?`
从而导致编译的带Xfce桌面的系统没有桌面等问题，
请将清华源更换为中科大源，即将以下文件
```
board/phytium/common/post-custom-skeleton-ubuntu.sh
board/phytium/common/ubuntu-package-installer
board/phytium/common/post-custom-skeleton-debian-11.sh
board/phytium/common/debian-package-installer
```
中的`mirrors.tuna.tsinghua.edu.cn`改为`mirrors.ustc.edu.cn`

2. E2000需要安装软件包来启用VPU功能，请咨询linan1284@phytium.com.cn获取软件包。  

3. 在Ubuntu或Debian上安装完cpufreq-utils，CPU调频模式不再是performance。  
安装了cpufreq-utils以后，会同时安装一个/etc/init.d/cpufrequtils脚本，这个脚本在启动后会将cpufreq的模式设置为ondemand动态调频。
可以把这个脚本改一下：将ENABLE改为false，便不会修改cpufreq策略，或者将GOVERNOR=“performance”。  
4. 在Ubuntu20.04或Debian11主机系统编译Ubuntu22.04报错
```
E: No such script: /usr/share/debootstrap/scripts/jammy
installing for second-stage ...
chroot: failed to runcommand '/debootstrap/debootstrap': No such file or directory
```
报错原因是安装的debootstrap软件包版本缺少/usr/share/debootstrap/scripts/jammy文件，需要重新安装debootstrap。  
对于Ubuntu20.04主机系统，需要卸载重新安装debootstrap：
```
$ sudo apt --purge remove debootstrap
$ sudo apt update
$ sudo apt install debootstrap
```
对于Debian11主机系统，需要从Debian11 backports源安装debootstrap：
```
$ sudo apt update
$ sudo apt install -t bullseye-backports debootstrap
```
