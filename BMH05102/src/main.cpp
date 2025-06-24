#include <HardwareSerial.h>

// BMH05102 ENABLE引脚定义
#define BMH_ENABLE_PIN 2  // 使用GPIO2作为ENABLE控制引脚

HardwareSerial BHM(1);

// 人体成分参数结构
struct BodyComposition {
  float bodyFat;        // 体脂率 (%)
  float muscleMass;     // 肌肉量 (kg)
  float boneMass;       // 骨量 (kg)
  float waterContent;   // 水分含量 (%)
  float visceralFat;    // 内脏脂肪等级
  float bmr;            // 基础代谢率 (kcal)
  float bodyAge;        // 体年龄
  float proteinRate;    // 蛋白质率 (%)
  float subcutaneousFat; // 皮下脂肪量 (kg)
  float bmi;            // BMI指数
};

// 函数声明
uint8_t calculateChecksum(uint8_t* data, uint8_t length);
bool sendCommand(uint8_t* command, uint8_t cmdLen, uint8_t* response, uint8_t expectedLen, const char* cmdName);
bool setHandImpedanceMode(bool enable);
bool readImpedanceMode();
bool enterImpedanceMode();
void queryImpedanceStatus();
bool getBodyCompositionData(uint16_t impedance, BodyComposition& result);
bool getBodyCompositionLevel();
void setBMHEnable(bool enable);

// 用户信息 (用于计算人体成分)
struct UserInfo {
  uint8_t age;          // 年龄
  uint8_t gender;       // 性别 (0=女, 1=男)
  float height;         // 身高 (cm)
  float weight;         // 体重 (kg)
  uint8_t activity;     // 活动等级 (1-5)
};

// 设置用户信息 (请根据实际情况修改)
UserInfo user = {
  .age = 21,
  .gender = 1,        // 1=男性, 0=女性
  .height = 175.0,    // 身高 170cm
  .weight = 64.0,     // 体重 70kg
  .activity = 3       // 中等活动量
};

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("=== BMH05102 人体成分分析仪 ===");
  
  // 初始化ENABLE引脚
  pinMode(BMH_ENABLE_PIN, OUTPUT);
  setBMHEnable(true);  // 启用模块
  
  // 使用GPIO4(RX), GPIO5(TX)
  BHM.begin(9600, SERIAL_8N1, 4, 5);
  
  Serial.println("硬件连接:");
  Serial.println("BMH05102 TX -> ESP32-S3 GPIO4");
  Serial.println("BMH05102 RX -> ESP32-S3 GPIO5");
  Serial.println("BMH05102 VCC -> 3.3V");
  Serial.println("BMH05102 GND -> GND");
  Serial.println("BMH05102 ENABLE -> ESP32-S3 GPIO2");
  Serial.println();
  
  // 显示用户信息
  Serial.printf("👤 用户信息: %s, %d岁, %.1fcm, %.1fkg\n", 
                user.gender ? "男性" : "女性", user.age, user.height, user.weight);
  Serial.println();
  
  delay(3000); // 等待模块稳定
  
  // 清空串口缓冲区
  while (BHM.available()) {
    BHM.read();
  }
    Serial.println("✅ 初始化完成，开始人体成分分析...\n");
  
  // 读取当前测量模式
  delay(1000);
  readImpedanceMode();
  
  // 设置为双手测量模式（可选）
  delay(1000);
  setHandImpedanceMode(true);  // 取消注释以启用双手模式
  
  // 再次确认当前模式
  delay(1000);
  readImpedanceMode();
}

// 计算校验位
uint8_t calculateChecksum(uint8_t* data, uint8_t length) {
  uint8_t checksum = 0;
  for (int i = 1; i < length - 2; i++) {
    checksum ^= data[i];
  }
  return checksum;
}

// 发送命令并等待响应
bool sendCommand(uint8_t* command, uint8_t cmdLen, uint8_t* response, uint8_t expectedLen, const char* cmdName) {
  // 清空接收缓冲区
  while (BHM.available()) {
    BHM.read();
  }
  
  // 发送命令
  Serial.printf("📤 发送%s: ", cmdName);
  for (int i = 0; i < cmdLen; i++) {
    Serial.printf("%02X ", command[i]);
  }
  Serial.println();
  
  BHM.write(command, cmdLen);
  BHM.flush();
  
  // 等待响应
  unsigned long startTime = millis();
  int bytesRead = 0;
  
  while (millis() - startTime < 500 && bytesRead < expectedLen) {
    if (BHM.available()) {
      response[bytesRead] = BHM.read();
      bytesRead++;
      delay(2);
    }
    delay(1);
  }
  
  if (bytesRead == expectedLen) {
    Serial.printf("📥 收到响应(%d字节): ", bytesRead);
    for (int i = 0; i < bytesRead; i++) {
      Serial.printf("%02X ", response[i]);
    }
    Serial.println();
    return true;
  } else {
    Serial.printf("❌ 响应错误，收到%d字节，期望%d字节\n", bytesRead, expectedLen);
    return false;
  }
}



// 显示人体成分分析结果
void displayBodyComposition(BodyComposition& comp) {
  Serial.println("\n🧬 人体成分分析结果:");
  Serial.println("================================");
    Serial.printf("💧 水分含量: %.1f%%\n", comp.waterContent);
  Serial.printf("🥩 体脂率: %.1f%%", comp.bodyFat);
  
  // 体脂率评价
  if (user.gender == 1) { // 男性标准
    if (comp.bodyFat < 6) Serial.println(" (偏瘦)");
    else if (comp.bodyFat < 14) Serial.println(" (理想)");
    else if (comp.bodyFat < 18) Serial.println(" (正常)");
    else if (comp.bodyFat < 25) Serial.println(" (偏胖)");
    else Serial.println(" (肥胖)");
  } else { // 女性标准
    if (comp.bodyFat < 16) Serial.println(" (偏瘦)");
    else if (comp.bodyFat < 21) Serial.println(" (理想)");
    else if (comp.bodyFat < 25) Serial.println(" (正常)");
    else if (comp.bodyFat < 32) Serial.println(" (偏胖)");
    else Serial.println(" (肥胖)");
  }
  
  Serial.printf("💪 肌肉量: %.1f kg\n", comp.muscleMass);
  Serial.printf("🦴 骨量: %.1f kg\n", comp.boneMass);
  Serial.printf("🫀 内脏脂肪: %.0f级", comp.visceralFat);
  
  // 内脏脂肪评价
  if (comp.visceralFat <= 9) Serial.println(" (正常)");
  else if (comp.visceralFat <= 14) Serial.println(" (偏高)");
  else Serial.println(" (过高)");
  
  Serial.printf("🔥 基础代谢: %.0f kcal/天\n", comp.bmr);
  Serial.printf("🎂 体年龄: %.0f 岁\n", comp.bodyAge);
  Serial.printf("💊 蛋白质率: %.1f%%\n", comp.proteinRate);
  Serial.printf("🧈 皮下脂肪: %.1f kg\n", comp.subcutaneousFat);
  Serial.printf("⚖️ BMI指数: %.1f\n", comp.bmi);
  
  Serial.println("================================");
}

// 解析阻抗数据并计算人体成分
void parseImpedanceData(uint8_t* data) {
  Serial.println("\n📊 阻抗测量结果:");
  
  // 工作状态2 (字节5): 阻抗测量状态
  uint8_t workStatus2 = data[5];
  uint8_t impedanceStatus = (workStatus2 >> 4) & 0x0F;
  
  Serial.printf("阻抗状态: %d ", impedanceStatus);
  switch(impedanceStatus) {
    case 0: 
      Serial.println("(测量未进行)"); 
      return;
    case 1: 
      Serial.println("(阻抗测量中...)"); 
      return;
    case 2: 
      Serial.println("(阻抗测量成功)"); 
      break;
    case 3: 
      Serial.println("(阻抗测量失败)"); 
      return;
    default: 
      Serial.println("(未知状态)"); 
      return;
  }
  
  // 原始阻抗值 (字节9-10)
  uint16_t rawImpedance = (data[9] << 8) | data[10];
  
  if (rawImpedance == 0xFFFF || rawImpedance == 0xFFF1 || rawImpedance == 0xFFF2) {
    Serial.println("❌ 阻抗测量失败 - 请检查人体接触");
    return;
  } else if (rawImpedance == 0x0000) {
    Serial.println("⏳ 阻抗数据为零 - 可能还在测量中");
    return;
  } else {
    Serial.printf("✅ 原始阻抗: %d Ω\n", rawImpedance);    // 使用模块内置算法计算人体成分
    BodyComposition composition = {0};
    if (getBodyCompositionData(rawImpedance, composition)) {
      displayBodyComposition(composition);
      
      // 获取等级判断
      delay(500);
      getBodyCompositionLevel();
    } else {
      Serial.println("❌ 模块算法失败，无法计算人体成分");
    }
  }
  
  Serial.println("---");
}

// 进入阻抗测量模式
bool enterImpedanceMode() {
  uint8_t command[] = {0xA5, 0x05, 0x26, 0x10, 0x03, 0x30, 0xAA};
  uint8_t response[7];
  
  if (sendCommand(command, sizeof(command), response, sizeof(response), "阻抗模式")) {
    if (response[0] == 0x5A && response[4] == 0x03) {
      Serial.println("✅ 成功进入阻抗模式");
      return true;
    } else if (response[4] == 0xFF) {
      Serial.println("❌ 阻抗模式错误响应");
      return false;
    }
  }
  return false;
}

// 查询状态获取阻抗数据
void queryImpedanceStatus() {
  uint8_t command[] = {0xA5, 0x05, 0x26, 0x11, 0x00, 0x32, 0xAA};
  uint8_t response[18];
  
  if (sendCommand(command, sizeof(command), response, sizeof(response), "状态查询")) {
    if (response[0] == 0x5A && response[1] == 0x10) {
      parseImpedanceData(response);
    } else {
      Serial.println("❌ 状态查询响应格式错误");
    }
  }
}

// 设置双手阻抗测量模式
bool setHandImpedanceMode(bool enable) {
  uint8_t enableValue = enable ? 0x01 : 0x00;
  uint8_t command[] = {0xA5, 0x0A, 0x26, 0x14, 0x02, 0x2D, 0x00, 0x00, 0x00, enableValue, 0x00, 0xAA};
  
  // 计算校验位
  command[10] = calculateChecksum(command, sizeof(command));
  
  uint8_t response[12];
  
  if (sendCommand(command, sizeof(command), response, sizeof(response), enable ? "设置双手模式" : "设置双脚模式")) {
    if (response[0] == 0x5A && response[4] == 0x00) {
      Serial.printf("✅ 成功设置为%s\n", enable ? "双手阻抗测量模式" : "双脚阻抗测量模式");
      return true;
    } else {
      Serial.printf("❌ 设置失败，错误代码: 0x%02X\n", response[4]);
      return false;
    }
  }
  return false;
}

// 读取当前测量模式
bool readImpedanceMode() {
  uint8_t command[] = {0xA5, 0x0A, 0x26, 0x14, 0x01, 0x2D, 0x00, 0x00, 0x00, 0x00, 0x14, 0xAA};
  uint8_t response[12];
  
  if (sendCommand(command, sizeof(command), response, sizeof(response), "读取测量模式")) {
    if (response[0] == 0x5A && response[4] == 0x00) {
      uint8_t mode = response[9]; // 参数值的最低位
      Serial.printf("📋 当前测量模式: %s\n", mode ? "双手阻抗测量" : "双脚阻抗测量");
      return mode;
    }
  }
  return false;
}

// 获取人体成分数据 (使用模块内置算法 - 命令0x15)
bool getBodyCompositionData(uint16_t impedance, BodyComposition& result) {
  if (impedance == 0 || impedance >= 0xFFF0) {
    Serial.println("❌ 无效阻抗值，无法计算人体成分");
    return false;
  }
  
  // 参数验证
  if (user.height < 90 || user.height > 220) {
    Serial.println("❌ 身高超出范围 (90-220cm)");
    return false;
  }
  if (user.weight < 10.0 || user.weight > 200.0) {
    Serial.println("❌ 体重超出范围 (10.0-200.0kg)");
    return false;
  }
  if (user.age < 6 || user.age > 99) {
    Serial.println("❌ 年龄超出范围 (6-99岁)");
    return false;
  }
  
  // 构建命令 0x15
  uint8_t height = (uint8_t)user.height;                    // 身高 (90~220cm)
  uint16_t weight = (uint16_t)(user.weight * 10);          // 体重 (100-2000, 对应10.0-200.0kg)
  uint8_t age = user.age;                                   // 年龄 (6~99岁)
  uint8_t gender = user.gender;                             // 性别 (0=女, 1=男)
  
  uint8_t command[] = {
    0xA5,                                    // 帧头
    0x0B,                                    // 长度
    0x26,                                    // 设备类型
    0x15,                                    // 命令号
    height,                                  // 身高
    (uint8_t)(weight >> 8),                  // 体重高位
    (uint8_t)(weight & 0xFF),                // 体重低位
    age,                                     // 年龄
    gender,                                  // 性别
    (uint8_t)(impedance >> 8),              // 阻抗高位
    (uint8_t)(impedance & 0xFF),            // 阻抗低位
    0x00,                                    // 校验位 (稍后计算)
    0xAA                                     // 帧尾
  };
  
  // 计算校验位
  command[11] = calculateChecksum(command, sizeof(command));
  
  uint8_t response[24]; // 应答长度为0x16(22) + 帧头 + 长度 = 24字节
  
  Serial.printf("\n📤 发送人体成分计算请求 (身高:%dcm, 体重:%.1fkg, 年龄:%d岁, 性别:%s, 阻抗:%dΩ)\n", 
                height, user.weight, age, gender ? "男" : "女", impedance);
  
  if (sendCommand(command, sizeof(command), response, sizeof(response), "人体成分计算")) {
    if (response[0] == 0x5A && response[1] == 0x16) {
      
      // 检查错误状态
      uint8_t errorCode = response[4];
      if (errorCode != 0x00) {
        Serial.printf("❌ 计算失败，错误代码: 0x%02X ", errorCode);
        switch(errorCode) {
          case 0x01: Serial.println("(阻抗数据出错)"); break;
          case 0x02: Serial.println("(年龄错误)"); break;
          case 0x03: Serial.println("(身高错误)"); break;
          case 0x04: Serial.println("(体重错误)"); break;
          case 0x05: Serial.println("(性别错误)"); break;
          default: Serial.println("(未知错误)"); break;
        }
        return false;
      }
      
      // 解析人体成分数据 (按协议文档格式)
      uint16_t fatRate = (response[5] << 8) | response[6];         // 脂肪率
      uint16_t waterRate = (response[7] << 8) | response[8];       // 水分率
      uint16_t muscleRate = (response[9] << 8) | response[10];     // 肌肉率
      uint8_t boneWeight = response[11];                           // 骨量
      uint16_t bmr = (response[12] << 8) | response[13];          // 基础代谢
      uint8_t visceralFat = response[14];                         // 内脏脂肪
      uint16_t bmi = (response[15] << 8) | response[16];          // BMI
      uint8_t bodyAge = response[17];                             // 体年龄
      uint16_t proteinRate = (response[18] << 8) | response[19];  // 蛋白质率
      uint16_t subcutFat = (response[20] << 8) | response[21];    // 皮下脂肪量
      
      // 转换为实际数值 (根据协议分辨率)
      result.bodyFat = fatRate / 10.0;                // 分辨率0.1%
      result.waterContent = waterRate / 10.0;         // 分辨率0.1%
      float muscleRatePercent = muscleRate / 10.0;    // 肌肉率百分比
      result.boneMass = boneWeight / 10.0;            // 分辨率0.1kg
      result.bmr = bmr;                               // 分辨率1
      result.visceralFat = visceralFat;               // 分辨率1
      result.bmi = bmi / 10.0;                        // 分辨率0.1
      result.bodyAge = bodyAge;                       // 分辨率1
      result.proteinRate = proteinRate / 10.0;        // 分辨率0.1%
      result.subcutaneousFat = subcutFat / 10.0;      // 分辨率0.1kg
      
      // 从肌肉率计算实际肌肉量
      result.muscleMass = (muscleRatePercent / 100.0) * user.weight;
      
      Serial.println("✅ 模块内置算法计算成功！");
      return true;
      
    } else {
      Serial.println("❌ 响应格式错误");
      return false;
    }
  }
  
  return false;
}

// 获取人体成分等级判断 (命令0x16)
bool getBodyCompositionLevel() {
  uint8_t command[] = {0xA5, 0x04, 0x26, 0x16, 0x34, 0xAA};
  uint8_t response[16]; // 应答长度为0x0E(14) + 帧头 + 长度 = 16字节
  
  if (sendCommand(command, sizeof(command), response, sizeof(response), "人体成分等级判断")) {
    if (response[0] == 0x5A && response[1] == 0x0E) {
      
      // 检查错误状态
      uint8_t errorCode = response[4];
      if (errorCode != 0x00) {
        Serial.printf("❌ 等级判断失败，错误代码: 0x%02X\n", errorCode);
        return false;
      }
      
      // 解析等级数据
      uint8_t fatLevel = response[5];      // 脂肪率等级
      uint8_t waterLevel = response[6];    // 水分率等级
      uint8_t muscleLevel = response[7];   // 肌肉率等级
      uint8_t boneLevel = response[8];     // 骨量等级
      uint8_t bmrLevel = response[9];      // 基础代谢等级
      uint8_t visceralLevel = response[10]; // 内脏脂肪等级
      uint8_t bmiLevel = response[11];     // BMI等级
      uint8_t proteinLevel = response[12]; // 蛋白质率等级
      uint8_t subcutLevel = response[13];  // 皮下脂肪量等级
      
      Serial.println("\n📊 人体成分等级评估:");
      Serial.println("================================");
      
      // 脂肪率等级
      Serial.printf("🥩 体脂率等级: ");
      switch(fatLevel) {
        case 0: Serial.println("偏瘦"); break;
        case 1: Serial.println("标准"); break;
        case 2: Serial.println("警惕"); break;
        case 3: Serial.println("偏胖"); break;
        case 4: Serial.println("肥胖"); break;
        default: Serial.println("未知"); break;
      }
      
      // 水分率等级
      Serial.printf("💧 水分率等级: ");
      switch(waterLevel) {
        case 0: Serial.println("不足"); break;
        case 1: Serial.println("标准"); break;
        case 2: Serial.println("优秀"); break;
        default: Serial.println("未知"); break;
      }
      
      // 肌肉率等级
      Serial.printf("💪 肌肉率等级: ");
      switch(muscleLevel) {
        case 0: Serial.println("不足"); break;
        case 1: Serial.println("标准"); break;
        case 2: Serial.println("优秀"); break;
        default: Serial.println("未知"); break;
      }
      
      // 骨量等级
      Serial.printf("🦴 骨量等级: ");
      switch(boneLevel) {
        case 0: Serial.println("不足"); break;
        case 1: Serial.println("标准"); break;
        case 2: Serial.println("优秀"); break;
        default: Serial.println("未知"); break;
      }
      
      // 基础代谢等级
      Serial.printf("🔥 基础代谢等级: ");
      switch(bmrLevel) {
        case 0: Serial.println("偏低"); break;
        case 1: Serial.println("达标"); break;
        default: Serial.println("未知"); break;
      }
      
      // 内脏脂肪等级
      Serial.printf("🫀 内脏脂肪等级: ");
      switch(visceralLevel) {
        case 0: Serial.println("标准"); break;
        case 1: Serial.println("警惕"); break;
        case 2: Serial.println("危险"); break;
        default: Serial.println("未知"); break;
      }
      
      // BMI等级
      Serial.printf("⚖️ BMI等级: ");
      switch(bmiLevel) {
        case 0: Serial.println("偏瘦"); break;
        case 1: Serial.println("普通"); break;
        case 2: Serial.println("偏胖"); break;
        case 3: Serial.println("肥胖"); break;
        default: Serial.println("未知"); break;
      }
      
      // 蛋白质率等级
      Serial.printf("💊 蛋白质率等级: ");
      switch(proteinLevel) {
        case 0: Serial.println("不足"); break;
        case 1: Serial.println("标准"); break;
        case 2: Serial.println("优秀"); break;
        default: Serial.println("未知"); break;
      }
      
      // 皮下脂肪量等级
      Serial.printf("🧈 皮下脂肪量等级: ");
      switch(subcutLevel) {
        case 0: Serial.println("不足"); break;
        case 1: Serial.println("标准"); break;
        case 2: Serial.println("高"); break;
        default: Serial.println("未知"); break;
      }
      
      Serial.println("================================");
      return true;
      
    } else {
      Serial.println("❌ 等级判断响应格式错误");
      return false;
    }
  }
  
  return false;
}

// ENABLE引脚控制函数
void setBMHEnable(bool enable) {
  digitalWrite(BMH_ENABLE_PIN, enable ? HIGH : LOW);
  if (enable) {
    Serial.println("✅ BMH05102模块已使能");
    delay(100); // 等待模块稳定
  } else {
    Serial.println("💤 BMH05102模块已禁用");
  }
}

void loop() {
  static int testStep = 0;
  static unsigned long lastTest = 0;
  static bool impedanceModeActive = false;
  
  if (millis() - lastTest < 4000) return; // 4秒间隔，给足够测量时间
  lastTest = millis();
  
  switch (testStep) {
    case 0:
      Serial.println("\n=== 🔄 开始人体成分分析 ===");
      setBMHEnable(true);  // 确保模块使能
      impedanceModeActive = enterImpedanceMode();
      if (impedanceModeActive) {
        // 根据当前模式显示不同提示
        bool isHandMode = readImpedanceMode();
        if (isHandMode) {
          Serial.println("💡 请双手紧握手柄电极");
          Serial.println("💡 保持双手下垂伸直，身体稳定");
        } else {
          Serial.println("💡 请双脚稳定站在称重台电极上");
          Serial.println("💡 保持身体挺直，双臂自然下垂");
        }
        Serial.println("⏳ 正在测量阻抗，请稍等...");
      }
      break;
      
    case 1:
    case 2:
    case 3:
      if (impedanceModeActive) {
        Serial.printf("\n=== 📊 阻抗测量进度 %d/3 ===\n", testStep);
        queryImpedanceStatus();
      } else {
        testStep = -1; // 重新开始
      }
      break;
      
    default:
      testStep = -1; // 重新开始
      impedanceModeActive = false;
      Serial.println("\n=== 🔄 准备下一次测量 ===");
      delay(2000); // 多等2秒再开始下次测量
      break;
  }
  
  testStep++;
}
