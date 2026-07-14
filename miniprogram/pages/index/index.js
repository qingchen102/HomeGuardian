// pages/index/index.js
const mqtt = require('../../utils/mqtt.min.js')
import * as echarts from '../../ec-canvas/echarts'; 

// ==========================================
// 🚨🚨🚨 开发者必填区 🚨🚨🚨
// ==========================================
// 1. 微信云开发环境配置
const CLOUD_ENV_ID = "cloud1-填入你的真实环境ID"; // 去云开发控制台-设置里找

// 2. 华为云配置
const DEVICE_ID = "你的华为云设备ID";
const HUAWEI_ENDPOINT = "你的华为云纯英文域名"; 
const CLIENT_ID = "华为云生成的ClientId"; 
const USERNAME = "华为云生成的Username";
const PASSWORD = "华为云生成的Password";

// 3. AI 大模型配置
const AI_API_URL = "[https://api.deepseek.com/chat/completions](https://api.deepseek.com/chat/completions)"; 
const AI_API_KEY = "sk-你的大模型API密钥"; 
// ==========================================

let chartInstance = null;
let lastSaveTime = 0; // 记录上次存库的时间

Page({
  data: {
    deviceOnline: false, 
    bedside: { present: false, heartRate: '--', breathRate: '--' },
    livingRoom: { present: false, heartRate: '--', spo2: '--' },
    emergency: { active: false, reason: '' },
    aiReport: '',
    aiLoading: false,
    client: null,
    ec: { lazyLoad: true },
    heartRateHistory: [] 
  },

  watchdogTimer: null,

  onLoad: function () {
    const that = this;
    
    // 【核心新增 1】：初始化微信云开发环境并拉取历史数据
    wx.cloud.init({ env: CLOUD_ENV_ID, traceUser: true });
    this.db = wx.cloud.database();
    
    // 初始化 ECharts 组件
    this.echartsComponnet = this.selectComponent('#mychart-dom-line');
    
    // 先尝试从云端加载历史数据，加载完毕后再连华为云
    this.loadHistoryDataFromCloud(() => {
      that.initChart();
      that.connectHuaweiCloud();
    });
  },

  // ==========================================
  // 🌟 数据持久化功能区 (云数据库读写)
  // ==========================================
  
  // 读数据：从云数据库拉取历史心率记录
  loadHistoryDataFromCloud(callback) {
    const that = this;
    wx.showLoading({ title: '加载历史记录' });
    
    this.db.collection('health_log')
      .orderBy('createTime', 'desc') // 按时间倒序拉取最新数据
      .limit(20) // 最多拉20个点画图
      .get({
        success: res => {
          if (res.data.length > 0) {
            // 云端取回的是倒序的，我们需要反转回来用于折线图从左到右显示
            let history = res.data.map(item => item.heartRate).reverse();
            that.setData({ heartRateHistory: history });
            console.log("成功拉取云端历史数据:", history);
          }
          wx.hideLoading();
          callback(); // 必须调用回调，以进行下一步图表渲染
        },
        fail: err => {
          console.error("读取云数据库历史记录失败 (请检查是否建了health_log集合及权限设置)", err);
          wx.hideLoading();
          callback();
        }
      });
  },

  // 写数据：将最新心率存入云数据库 (带限流保护)
  saveDataToCloud(heartRate) {
    if (heartRate === '--') return;
    const now = Date.now();
    
    // 节流阀：限制每 15 秒才存一次云数据库，防止瞬间并发太高把免费额度耗尽
    if (now - lastSaveTime > 15000) {
      lastSaveTime = now;
      this.db.collection('health_log').add({
        data: {
          heartRate: heartRate,
          createTime: this.db.serverDate() // 记录存入云端的标准时间
        },
        success: res => console.log('【云持久化】心率数据已上云', heartRate),
        fail: err => console.error('云数据存储失败', err)
      });
    }
  },

  // ==========================================
  // ECharts 与业务逻辑区
  // ==========================================

  initChart: function () {
    this.echartsComponnet.init((canvas, width, height, dpr) => {
      const chart = echarts.init(canvas, null, { width: width, height: height, devicePixelRatio: dpr });
      chartInstance = chart;
      
      // 如果云端有历史数据就用历史数据，没有就用空数组占位
      const initialData = this.data.heartRateHistory.length > 0 
                          ? this.data.heartRateHistory 
                          : [75, 75, 75, 75, 75, 75, 75, 75, 75, 75];

      const option = {
        grid: { top: 20, right: 10, bottom: 20, left: 30, containLabel: false },
        xAxis: { type: 'category', boundaryGap: false, data: Array(20).fill(''), show: false },
        yAxis: { type: 'value', min: 40, max: 140, splitLine: { lineStyle: { type: 'dashed', color: '#e2e8f0' } } },
        series: [{
          type: 'line',
          smooth: true, 
          data: initialData, 
          itemStyle: { color: '#f43f5e' },
          areaStyle: { color: new echarts.graphic.LinearGradient(0, 0, 0, 1, [{ offset: 0, color: 'rgba(244,63,94,0.4)' }, { offset: 1, color: 'rgba(244,63,94,0)' }]) }
        }]
      };
      chart.setOption(option);
      return chart;
    });
  },

  updateChartData: function(newHeartRate) {
    if (!chartInstance || newHeartRate === '--') return;
    
    let history = this.data.heartRateHistory;
    history.push(newHeartRate);
    if (history.length > 20) history.shift(); 
    
    this.setData({ heartRateHistory: history });
    chartInstance.setOption({ series: [{ data: history }] });

    // 【核心新增 2】：触发存库逻辑
    this.saveDataToCloud(newHeartRate);
  },

  feedWatchdog: function() {
    this.setData({ deviceOnline: true });
    if (this.watchdogTimer) clearTimeout(this.watchdogTimer);
    this.watchdogTimer = setTimeout(() => {
      this.setData({ deviceOnline: false });
      wx.showToast({ title: '设备已掉线', icon: 'error' });
    }, 30000); 
  },

  connectHuaweiCloud() {
    const that = this;
    const options = { 
      protocol: 'wxs',           
      hostname: HUAWEI_ENDPOINT, port: 443, path: '/mqtt',             
      clientId: CLIENT_ID, username: USERNAME, password: PASSWORD, 
      protocolVersion: 4, clean: false 
    };

    const client = mqtt.connect(options);
    
    client.on('connect', () => {
      wx.showToast({ title: '云平台已接入', icon: 'success' });
      client.subscribe(`$oc/devices/${DEVICE_ID}/sys/properties/report`);
      that.feedWatchdog();
    });

    client.on('message', (topic, message) => {
      const payload = JSON.parse(message.toString());
      that.feedWatchdog();

      if (payload.services && payload.services.length > 0) {
        payload.services.forEach(service => {
          const props = service.properties;
          
          if (service.service_id === 'Bedside') {
            const hr = props.heart_rate || that.data.bedside.heartRate;
            that.setData({
              'bedside.present': props.present === 1,
              'bedside.heartRate': hr,
              'bedside.breathRate': props.breath_rate || that.data.bedside.breathRate
            });
            
            that.updateChartData(hr);
            
            if (props.present === 1 && (props.heart_rate > 120 || props.heart_rate < 50)) {
              that.triggerEmergency('床头监测到心率异常（' + props.heart_rate + 'bpm）');
            }
          }
          else if (service.service_id === 'LivingRoom') {
            that.setData({
              'livingRoom.present': props.present === 1,
              'livingRoom.heartRate': props.heart_rate || that.data.livingRoom.heartRate,
              'livingRoom.spo2': props.spo2 || that.data.livingRoom.spo2
            });
            if (props.fall_status === 1) {
              that.triggerEmergency('客厅雷达检测到跌倒动作！');
            }
          }
        });
      }
    });
    this.setData({ client: client });
  },

  requestSubscribeMessage() {
    wx.requestSubscribeMessage({
      tmplIds: ['填写你申请到的真实微信模板ID'], 
      success(res) {
        if (res['填写你申请到的真实微信模板ID'] === 'accept') {
          wx.showToast({ title: '授权成功，设备跌倒时将通知您', icon: 'none', duration: 3000 });
        } else {
          wx.showToast({ title: '已取消授权', icon: 'error' });
        }
      },
      fail(err) {
        wx.showToast({ title: '请在真机环境下测试订阅功能', icon: 'none' });
      }
    })
  },

  generateAIReport() {
    const that = this;
    if (this.data.aiLoading) return;
    this.setData({ aiLoading: true, aiReport: 'AI 正在读取传感器数据并进行推演...' });
    const { bedside, livingRoom } = this.data;
    const prompt = `你是健康监护AI助手。基于数据(床头:${bedside.present?'有人':'无人'}, 心率${bedside.heartRate}bpm; 客厅:${livingRoom.present?'有人':'无人'}, 心率${livingRoom.heartRate}bpm)用50字给出简明健康评估。`;

    wx.request({
      url: AI_API_URL, method: 'POST',
      header: { 'Content-Type': 'application/json', 'Authorization': `Bearer ${AI_API_KEY}` },
      data: { model: 'deepseek-chat', messages: [{"role": "user", "content": prompt}], max_tokens: 100, temperature: 0.7 },
      success: function(res) {
        if (res.data.choices) that.setData({ aiReport: res.data.choices[0].message.content, aiLoading: false });
      },
      fail: function() { that.setData({ aiReport: 'AI 请求失败。', aiLoading: false }); }
    });
  },

  triggerEmergency(reasonText) {
    if (this.data.emergency.active) return; 
    this.setData({ emergency: { active: true, reason: reasonText } });
    wx.vibrateLong();
  },
  clearEmergency() {
    this.setData({ 'emergency.active': false });
    this.sendControlCommand("SystemControl", "ClearAlarm", 1);
  },
  callEmergencyContact() { wx.makePhoneCall({ phoneNumber: '120' }); },
  playEmergencyAudio() { this.sendControlCommand("AudioControl", "PlaySound", 1); },

  sendControlCommand(serviceId, commandName, value) {
    if (!this.data.client) return;
    const pubTopic = `$oc/devices/${DEVICE_ID}/sys/commands/request_id=${new Date().getTime()}`;
    const payload = { service_id: serviceId, command_name: commandName, paras: { [commandName]: value } };
    this.data.client.publish(pubTopic, JSON.stringify(payload));
  }
})


