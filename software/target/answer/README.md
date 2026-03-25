# 图片识别问答 Demo（阿里云）

本目录提供一个最小可运行脚本：`photo_qa_demo.py`  
输入本地图片 + 问题，调用阿里云多模态模型，并在终端输出答案。

## 1) 安装依赖

```bash
pip install -r requirements.txt
```

## 2) 配置 `.env`

编辑同目录下的 `.env`：

```env
DASHSCOPE_API_KEY=sk-xxxxxxxxxxxxxxxx
QWEN_API_KEY=
QWEN_VL_MODEL=qwen-vl-plus
QWEN_BASE_URL=https://dashscope.aliyuncs.com/compatible-mode/v1
QWEN_IMAGE_PATH=C:/Users/xxx/Desktop/test.jpg
QWEN_QUESTION=这张图里有什么？
```

脚本会自动加载 `.env`，无需手动 `set` 环境变量。

## 3) 运行

```bash
python photo_qa_demo.py
```

可选参数：
- `--image`：覆盖 `.env` 的 `QWEN_IMAGE_PATH`
- `--question`：覆盖 `.env` 的 `QWEN_QUESTION`
- `--model`：默认读取 `.env` 的 `QWEN_VL_MODEL`
- `--base-url`：默认读取 `.env` 的 `QWEN_BASE_URL`
- `--api-key`：直接在命令行传 key（不推荐，建议环境变量）

## 4) 常见问题

- `未找到 API Key`：检查 `.env` 里的 `DASHSCOPE_API_KEY` 或 `QWEN_API_KEY`
- `图片不存在`：检查 `--image` 路径
- 请求报 401/403：确认 key 是否可用、模型是否开通
