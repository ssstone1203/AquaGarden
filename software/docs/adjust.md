1.对系统控制界面的内容进行美化，分成左右两个界面，左边上部分是机械臂相关功能，左边下部分是水泵的相关功能，右边只显示终端的运行状态，删除上边的机械臂相关数据等
2.把E:\code\fish-arm\AquaGarden\software\fish-arm下的三个.py代码进行合并，保留对应的功能
3.把dashboard仪表盘界面下的ai大模型分析模块进行实现，调用我自己的api：
{
  "env": {
    "ANTHROPIC_BASE_URL": "https://api.kimi.com/coding/",
    "ANTHROPIC_AUTH_TOKEN": "",
    "ANTHROPIC_MODEL": "kimi-for-coding",
    "ANTHROPIC_DEFAULT_SONNET_MODEL": "kimi-for-coding",
    "ANTHROPIC_DEFAULT_OPUS_MODEL": "kimi-for-coding",
    "ANTHROPIC_DEFAULT_HAIKU_MODEL": "kimi-for-coding"
  },
  "theme": "dark",
  "effortLevel": "high",
  "includeCoAuthoredBy": false
}