🏡 HomeGuardian | 智能监护小程序前端

专为独居老人设计的健康监护系统。本项目结合了华为云 IoTDA 物联网平台、微信云开发（数据持久化）以及大模型 AI 分析，实现了“云-管-端”的全链路闭环通信。

🌟 核心技术与功能列表

设备直连与低延迟通信：使用 WebSocket 封装 MQTT (wxs://)，前端绕过传统服务器，直接通过华为云与硬件（ESP32 等）实现毫秒级数据上下行双向通信。

ECharts 动态可视化：实时捕获心率数据，并渲染为平滑、可滚动的趋势折线图。

微信云数据库持久化：零服务器成本，通过 wx.cloud.database() 实现心率数据的定时上云（15秒节流机制），并在每次启动时自动拉取历史记录。

纯前端“看门狗”掉线检测：利用定时器机制（30秒阈值），无需硬件修改即可实现实时的设备断电/断网告警功能。

AI 智能报告生成：通过调用大语言模型 API（如 DeepSeek），融合多路传感器数据生成自然语言健康报告。

双重强力报警机制：硬件触发跌倒后，前端实现高斯模糊强制弹窗拦截、系统级长震动，并支持通过微信订阅消息实现锁屏推送。

📂 目录结构与外部依赖

📦 miniprogram
 ┣ 📂 ec-canvas            # [需手动下载] ECharts 微信小程序图表组件
 ┣ 📂 pages/index          # 监护大屏主页面（核心逻辑）
 ┃ ┣ 📜 index.js         # MQTT 通信、云开发读写、AI请求、ECharts 控制
 ┃ ┣ 📜 index.wxml       # 结构层（包含紧急弹窗、数据网格）
 ┃ ┗ 📜 index.wxss       # 样式层（蓝色沉浸渐变底色）
 ┗ 📂 utils
   ┗ 📜 mqtt.min.js        # 小程序适配版的 MQTT 库


⚠️ 依赖引入说明：
为避免仓库体积过大，ec-canvas 图表库需手动配置。请前往 echarts-for-weixin GitHub 下载，并将其中的 ec-canvas 文件夹放在 miniprogram/ 根目录下。

🛠️ 本地开发与联调配置

1. 核心参数填写

打开 pages/index/index.js，找到顶部的【开发者必填区】，需配置以下信息：

const CLOUD_ENV_ID = "cloud1-xxxx"; // 微信云开发环境 ID (需在云端先建好 health_log 集合)
const DEVICE_ID = "华为云设备ID";
const HUAWEI_ENDPOINT = "xxx.iot-mqtts.cn-north-4.myhuaweicloud.com"; // 纯英文域名
const CLIENT_ID = "华为云生成的 ClientId";
const USERNAME = "华为云生成的 Username";
const PASSWORD = "华为云生成的 Password";
const AI_API_KEY = "你的大模型 API 密钥";


2. 微信开发者工具设置

在开发者工具右上角点击 详情 -> 本地设置：

务必勾选：“不校验合法域名、web-view（业务域名）、TLS版本以及HTTPS证书”。（否则 MQTT 无法连接且 AI 请求会被拦截）。

📡 硬件通信协议 (To 硬件端同学)

前端按照华为云标准 Alink JSON 格式进行解析。硬件端通过 MQTT 向上报主题 $oc/devices/{device_id}/sys/properties/report 发送数据时，请严格遵守以下 JSON 结构：

🛏️ 床头点位 (Service ID: Bedside)

{
  "services": [{
    "service_id": "Bedside",
    "properties": {
      "present": 1,         // 1表示有人，0表示无人
      "heart_rate": 75,     // 实时心率 (整数)
      "breath_rate": 18     // 呼吸频率 (整数)
    }
  }]
}


🛋️ 客厅点位 (Service ID: LivingRoom)

{
  "services": [{
    "service_id": "LivingRoom",
    "properties": {
      "present": 1,
      "heart_rate": 80,
      "spo2": 98,           // 实时血氧
      "fall_status": 0      // ⚠️ 1: 跌倒报警(前端将强拉警报), 0: 正常
    }
  }]
}


⚙️ 前端下发指令 (需硬件端订阅处理)

解除警报 (蜂鸣器关)：SystemControl -> ClearAlarm (值为 1)

触发警报 (蜂鸣器开)：AudioControl -> PlaySound (值为 1)

