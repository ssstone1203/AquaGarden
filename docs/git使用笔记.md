我们从安装完成到将`git`添加到环境变量开始

# 设置git的用户名和邮箱（第一次使用，可选）

在任意位置打开`git`,然后执行如下命令进行设置

![image-20250405183458511](https://ssstone.oss-cn-beijing.aliyuncs.com/image-20250405183458511.png)

验证设置（按q退出）

```bash
git config --global --list
```



# 开启自己的项目

## 创建远程仓库

首先从0开始在`github`上创建一个仓库

<img src="https://ssstone.oss-cn-beijing.aliyuncs.com/image-20250405183917594.png" alt="image-20250405183917594" style="zoom:150%;" />

`.gitignore`用来忽略掉一些无关文件，执行`git add .`的时候不会被添加进暂存区

`license`是许可证，不同的许可证遵循不同的开源协议

![5d1b6712b8744768b405013c12a2461](https://ssstone.oss-cn-beijing.aliyuncs.com/5d1b6712b8744768b405013c12a2461.png)

## 把远程仓库和本地代码库绑定

### SSH模式

我们在刚刚创建好的远程仓库选择SSH连接，把提供的命令复制下来![image-20250405185140007](https://ssstone.oss-cn-beijing.aliyuncs.com/image-20250405185140007.png)

在`gitbash`输入指令

```shell
ssh-keygen -t rsa -C 你的邮箱
```

![image-20250405185414706](https://ssstone.oss-cn-beijing.aliyuncs.com/image-20250405185414706.png)

遇到冒号就按回车，直到出现图形

![image-20250405185605803](https://ssstone.oss-cn-beijing.aliyuncs.com/image-20250405185605803.png)

按照路径打开指定文件，全选复制

![image-20250405185721660](https://ssstone.oss-cn-beijing.aliyuncs.com/image-20250405185721660.png)

打开`github`，选择`settings`，进入`ssh`配置

![image-20250405185919400](https://ssstone.oss-cn-beijing.aliyuncs.com/image-20250405185919400.png)

此后，此台电脑只要使用`ssh`连接，就不需要校验权限



### HTTP模式（推荐）

==使用魔法，配置系统代理转发（改善 git clone 速度）==

```bash
git config --global http.proxy http://127.0.0.1:7890
git config --global https.proxy http://127.0.0.1:7890
```



```
git init  # 如果尚未初始化 git 仓库
git remote add origin https://github.com/username/my-project.git
```

如果`git init`之前文件夹有文件，别忘了

```bash
git init
git branch -M main  # -M强制重命名分支
git remote add origin https://github.com/ssstone1203/mydoc.git
git push -u origin main # 	设置跟踪关系（下次可以直接用 git push）
```



# 开发流程

## 单人开发

把刚刚创建的仓库`clone`到本地

![image-20250405191126620](https://ssstone.oss-cn-beijing.aliyuncs.com/image-20250405191126620.png)

查看当前分支

![image-20250405191336434](https://ssstone.oss-cn-beijing.aliyuncs.com/image-20250405191336434.png)

发现是`main(master)`用来保存稳定的代码

创建开发分支`develop`

![image-20250405191533741](https://ssstone.oss-cn-beijing.aliyuncs.com/image-20250405191533741.png)

在`develop`分支完成一个代码提交

![image-20250405191647393](https://ssstone.oss-cn-beijing.aliyuncs.com/image-20250405191647393.png)

合并提交

![image-20250405191852805](https://ssstone.oss-cn-beijing.aliyuncs.com/image-20250405191852805.png)

把代码提交到`github`

![image-20250405192035460](https://ssstone.oss-cn-beijing.aliyuncs.com/image-20250405192035460.png)



## 多人开发

在`develop`分支再新建分支，并完成开发，之后切换回`develop`

![image-20250405192549563](https://ssstone.oss-cn-beijing.aliyuncs.com/image-20250405192549563.png)

将远程仓库的`develop`分支`pull`到本地

![image-20250405193129266](https://ssstone.oss-cn-beijing.aliyuncs.com/image-20250405193129266.png)

当你开发的时候，别人也在开发，`pull`下来的是最新的代码，这样再进行合并，就形成了最新的`develop`

![image-20250405193452963](https://ssstone.oss-cn-beijing.aliyuncs.com/image-20250405193452963.png)

> 总结一下多人开发
>
> 首先在`develop`再新建分支，`add commit`完成开发
>
> 之后切换回`develop`但是不急着合并，而是先把最新的`develop`分支`pull`下来
>
> 在最新的`develop`合并分支，之后再上传到`github`
>
> **勤拉取、勤合并，小步提交，多沟通**，基本上就能把 Git 冲突的概率和复杂度降到最低。

# vscode插件

<img src="https://ssstone.oss-cn-beijing.aliyuncs.com/image-20250405194021761.png" alt="image-20250405194021761" style="zoom: 67%;" />

暂时还没搞明白怎么用，但是看起来功能很强大

![image-20250405194126777](https://ssstone.oss-cn-beijing.aliyuncs.com/image-20250405194126777.png)