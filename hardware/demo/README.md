# demo

## 概况

- `demo`文件夹包含了所有模块的测试代码，方便各个模块的测试和后续移植。

## 💡关于e2studio的一些操作

- ❓怎么添加头文件路径：网上有[解决方法](https://blog.csdn.net/qq_50654853/article/details/139506894#:~:text=%E8%AF%A5%E9%94%99%E8%AF%AF%E7%9A%84%E5%8E%9F%E5%9B%A0%E6%98%AF%E7%BC%96%E8%AF%91%E8%B7%AF%E5%BE%84%E4%B8%AD%E6%B2%A1%E6%9C%89%E6%B7%BB%E5%8A%A0xxx.h%E7%9A%84%E8%B7%AF%E5%BE%84%E3%80%82%20%E6%96%87%E7%AB%A0%E6%B5%8F%E8%A7%88%E9%98%85%E8%AF%BB1.1k%E6%AC%A1%EF%BC%8C%E7%82%B9%E8%B5%9E4%E6%AC%A1%EF%BC%8C%E6%94%B6%E8%97%8F6%E6%AC%A1%E3%80%82,C%2FC%2B%2B%E6%9E%84%E5%BB%BA-%3E%E8%AE%BE%E7%BD%AE-%3E%E5%B7%A5%E5%85%B7%E8%AE%BE%E7%BD%AE-%3ECompiler-%3ESource-%3E%E5%A4%8D%E5%88%B63%E4%B8%AD%E4%BF%A1%E6%81%AF-%3E%E7%82%B9%E5%87%BB4%E3%80%82%20%E6%AD%A4%E6%97%B6%E6%96%B0%E5%BB%BA%E7%9A%84source%E6%96%87%E4%BB%B6%E5%A4%B9%E5%B0%B1%E8%A2%AB%E5%8C%85%E5%90%AB%E5%9C%A8%E7%BC%96%E8%AF%91%E8%B7%AF%E5%BE%84%E4%B8%AD-%3E%E7%82%B9%E5%87%BB%E5%BA%94%E7%94%A8%E5%B9%B6%E5%85%B3%E9%97%AD-%3E%E9%87%8D%E6%96%B0%E6%9E%84%E5%BB%BA%E5%B7%A5%E7%A8%8B-%3E%E5%AE%8C%E5%B7%A5%E3%80%82%20%E7%9B%AE%E5%BD%95%E4%B8%AD%E6%B7%BB%E5%85%A5%E6%96%B0%E5%BB%BA%E6%96%87%E4%BB%B6%E5%A4%B9%E8%B7%AF%E5%BE%84%EF%BC%8C%E6%AF%94%E5%A6%82source%E6%96%87%E4%BB%B6%E5%A4%B9%E5%B0%B1%E6%98%AF%E6%88%91%E6%96%B0%E5%BB%BA%E7%9A%84%EF%BC%8C%E7%84%B6%E5%90%8E2%E5%8B%BE%E9%80%89%EF%BC%8C%E7%A1%AE%E5%AE%9A%E3%80%82)
- ❓FSP的API有哪些、怎么用：除了官方的FSP手册，在e2studio中我们可以这么做：在工程树中找到并双击打开`configuration.xml`→点击`stack`→打开你需要查看的模块的stack，在`属性`中找到`API Info`栏，在这里可以看到这个stack所有的API
- 💡快捷键的使用：`alt+/`代码提示、`ctrl+shift+f`自动整理代码格式（先手动框选需要整理的代码再`ctrl+shift+f`）

## 📄目录结构

````c
demo
├─ soil_sensor		 //土壤湿度传感器
├─ pump				//水泵模块
├─ pressure_sensor	 //压力传感器
├─ RGB_light		//三色RGB
└─ README.md
````