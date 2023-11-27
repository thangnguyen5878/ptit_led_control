// Định nghĩa template ID, tên và phiên bản firmware của Blynk
#define BLYNK_TEMPLATE_ID "TMPL6lwrbAEGk"
#define BLYNK_TEMPLATE_NAME "LedControl2"
#define BLYNK_FIRMWARE_VERSION "0.1.0"

#define BLYNK_PRINT Serial
//#define BLYNK_DEBUG 

#define APP_DEBUG

// Chọn board sử dụng hoặc cấu hình board tùy chỉnh trong Settings.h
//#define USE_SPARKFUN_BLYNK_BOARD
#define USE_NODE_MCU_BOARD
//#define USE_WITTY_CLOUD_BOARD
//#define USE_WEMOS_D1_MINI

// Định nghĩa các chân cho LED và cảm biến
#define LED_PIN D0  // Chân cho LED
#define LED2_PIN D2 // Chân cho LED thứ 2
int LED3_PIN = 14; // Chân cho LED thứ 3 (D5)

#define LIGHT_SENSOR_PIN A0 // Chân cho cảm biến ánh sáng
//#define PIR_SENSOR_PIN D1
int PIR_SENSOR_PIN = 5;  // Chân cho cảm biến chuyển động (D1)

// Định nghĩa các datastream cho Blynk
#define LED_BUTTON_DATASTREAM V0  // Button Widget (Integer): 0-1
#define LED_SLIDER_DATASTREAM V1  // Slider Widget (Integer): 0-255
#define LED_TIMER_DATASTREAM V2   // Timer Widget
#define LIGHT_SENSOR_DATASTREAM V3   
#define PIR_SENSOR_DATASTREAM V4   
#define PIR_SENSOR_VALUE_DATASTREAM V5   

#include <ESP8266WiFi.h>
#include "BlynkEdgent.h"
#include <TimeLib.h>

BlynkTimer timer;

// Biến cho Slider và Button
int slider;
bool button;  // Trạng thái nút nhấn ảo (0: tắt, 1: bật)

// Biến cho Timer
long timer_start = 0xFFFF;  // Thời điểm đèn bắt đầu sáng (tính bằng giây)
long timer_stop = 0xFFFF;   // Thời điểm đèn ngừng sáng (tính bằng giây)
unsigned char weekday_option; // Lựa chọn ngày trong tuần

// Biến cho cảm biến ánh sáng
int lightSensor;
int lightSensorPercentage;

// Biến cho cảm biến chuyển động
int pirSensor;

// Biến thời gian
long rtc_sec;  // Thời điểm hiện tại (tính bằng giây)
unsigned char day_of_week;

// Biến cho LED
bool led;                  // Trạng thái của LED (0: tắt, 1: bật)

// Biến quản lý cập nhật Blynk
bool update_blynk_status;  // update_blynk_status = 1, cập nhật trạng thái của LED cho nút nhấn ảo

// Biến kiểm soát Timer
bool is_timer_on;          // is_timer_on = 1, đèn sáng theo cài đặt của Timer

// #########################################################################################################
void setup() {
  Serial.begin(115200);
  delay(100);

  // Khởi tạo chân cho LED và cảm biến
  pinMode(LED_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);
  pinMode(LED3_PIN, OUTPUT);
  pinMode(LIGHT_SENSOR_PIN, INPUT);
  pinMode(PIR_SENSOR_PIN, INPUT);

  // Tắt tất cả LED
  digitalWrite(LED_PIN, LOW);
  digitalWrite(LED2_PIN, LOW);
  digitalWrite(LED3_PIN, LOW);

  // Khởi tạo Blynk và Timer
  BlynkEdgent.begin();
  timer.setInterval(10000L, checkTime);
}


void loop() {
    // led 1
  BlynkEdgent.run();
  timer.run();
  led_mng();
  blynk_update();

// TODO: Sửa lại đọc giá trị của cảm biến chuyển động từ 0 đến 100
    // led 2 - light sensor
  lightSensor = analogRead(LIGHT_SENSOR_PIN);  // Đọc giá trị của cảm biến ánh sáng (0-1023)
  lightSensorPercentage = map(lightSensor, 0, 1023, 100, 0);
  Blynk.virtualWrite(LIGHT_SENSOR_DATASTREAM, lightSensorPercentage);
  if (lightSensorPercentage <= 20) {
    digitalWrite(LED2_PIN, HIGH);  // Nếu ánh sáng thấp, bật LED
  } else {
    digitalWrite(LED2_PIN, LOW);   // Ngược lại, tắt LED
  }

  // led 3 - pir sensor
  pirSensor = digitalRead(PIR_SENSOR_PIN);
  if(pirSensor == HIGH ) {
    digitalWrite(LED3_PIN, HIGH);  // Nếu phát hiện chuyển động, bật LED và gửi tín hiệu đến Blynk
    Blynk.virtualWrite(PIR_SENSOR_DATASTREAM, HIGH);
    delay(1000);
  } else {
    digitalWrite(LED3_PIN, LOW);   // Ngược lại, tắt LED và gửi tín hiệu đến Blynk
    Blynk.virtualWrite(PIR_SENSOR_DATASTREAM, LOW);
  }  
}

// #########################################################################################################
// Button (Integer): 0-1
BLYNK_WRITE(LED_BUTTON_DATASTREAM) {
  int val = param.asInt();
  // Nếu không trong thời gian của timer, có thể dùng nút nhấn để điều khiển LED
  if (is_timer_on == 0)
    button = val;
  // khi đang trong thời gian của timer thì không thể nhấn nút ảo để điều khiển LED
  else
    update_blynk_status = 1;  // Cập nhật trạng thái của LED cho nút ảo
}

// Slider (Integer) 0 -> 255
BLYNK_WRITE(LED_SLIDER_DATASTREAM) {
  // Đọc giá trị của slider trên app Blynk
  slider = param.asInt();
  Serial.print(slider);
  // Nếu LED đang sáng thì mới có thể thay đổi độ sáng
  if (button == 1)
    analogWrite(LED_PIN, slider);
}

// Timer
BLYNK_WRITE(LED_TIMER_DATASTREAM) {
  unsigned char week_day;  // Biến tạm thời xử lý trong hàm

  TimeInputParam t(param);

  if (t.hasStartTime() && t.hasStopTime()) {
    // Xử lý thông tin thời gian từ widget Timer
    timer_start = (t.getStartHour() * 60 * 60) + (t.getStartMinute() * 60) + t.getStartSecond();
    timer_stop = (t.getStopHour() * 60 * 60) + (t.getStopMinute() * 60) + t.getStopSecond();

    Serial.println(String("Start Time: ") + t.getStartHour() + ":" + t.getStartMinute() + ":" + t.getStartSecond());

    Serial.println(String("Stop Time: ") + t.getStopHour() + ":" + t.getStopMinute() + ":" + t.getStopSecond());

    for (int i = 1; i <= 7; i++) {
      if (t.isWeekdaySelected(i)) {
        week_day |= (0x01 << (i - 1));
        Serial.println(String("Day ") + i + " is selected");
      } else {
        week_day &= (~(0x01 << (i - 1)));
      }
    }

    weekday_option = week_day;
  } else {
    timer_start = 0xFFFF;
    timer_stop = 0xFFFF;
  }
}


// #########################################################################################################
// Callback được gọi khi có sự thay đổi trong widget Internal RTC
BLYNK_WRITE(InternalPinRTC) {
  const unsigned long DEFAULT_TIME = 1357041600;  // Mốc thời gian mặc định: 1/1/2013
  unsigned long blynkTime = param.asLong();

  // Kiểm tra xem thời gian từ widget RTC có hợp lệ không
  if (blynkTime >= DEFAULT_TIME) {
    // Cập nhật thời gian của thiết bị từ widget RTC
    setTime(blynkTime);

    // Xác định ngày trong tuần
    day_of_week = weekday();

    // Chuyển đổi từ chuẩn 1-7 (Thứ hai -> Chủ nhật) thành 0-6 (Chủ nhật -> Thứ hai)
    if (day_of_week == 1)
      day_of_week = 7;
    else
      day_of_week -= 1;

    // Cập nhật giây tính từ nửa đêm
    rtc_sec = (hour() * 60 * 60) + (minute() * 60) + second();

    // In ra thông tin về thời gian
    Serial.println(blynkTime);
    Serial.println(String("RTC Server: ") + hour() + ":" + minute() + ":" + second());
    Serial.println(String("Day of Week: ") + weekday());
  }
}

// #########################################################################################################
// Callback được gọi khi thiết bị kết nối với server Blynk
BLYNK_CONNECTED() {
  // Gửi lệnh "rtc sync" để đồng bộ thời gian với server
  Blynk.sendInternal("rtc", "sync");
}

// #########################################################################################################
// Callback được gọi trong hàm loop để đồng bộ thời gian với server Blynk
void checkTime() {
  Blynk.sendInternal("rtc", "sync");
}

// #########################################################################################################
// Hàm quản lý thời gian và trạng thái đèn LED
void led_mng() {
  bool old_led_status;
  old_led_status = led;

  // Kiểm tra xem có đang trong khoảng thời gian được đặt bởi timer không
  if (timer_start != 0xFFFF && timer_stop != 0xFFFF) {
    if (
      (
        (
          ((timer_start <= timer_stop) && (rtc_sec >= timer_start) && (rtc_sec < timer_stop))
          || ((timer_start >= timer_stop) && ((rtc_sec >= timer_start) || (rtc_sec < timer_stop))))
        && (weekday_option == 0x00 || (weekday_option & (0x01 << (day_of_week - 1)))))) {
      is_timer_on = 1;  // Nếu đang trong khoảng thời gian được đặt bởi timer, đèn sẽ sáng
    } else
      is_timer_on = 0;  // Ngược lại, đèn sẽ tắt
  } else
    is_timer_on = 0;

  // Kiểm tra và cập nhật trạng thái của đèn và nút nhấn áo
  if (is_timer_on) {
    led = 1;       // Nếu đang trong thời gian của timer, đèn sẽ sáng
    button = 0;    // Không thể điều khiển đèn bằng nút nhấn áo khi đang trong thời gian của timer
  } else {
    led = button;  // Nếu không trong thời gian của timer, trạng thái của đèn sẽ được điều khiển bởi nút nhấn áo
  }

  // Kiểm tra xem trạng thái của đèn có thay đổi hay không để cập nhật Blynk
  if (old_led_status != led)
    update_blynk_status = 1;

  // Kiểm soát trạng thái của đèn trên thiết bị
  if (led == 1) {
    analogWrite(LED_PIN, slider);  // Đèn sáng với độ sáng được điều chỉnh bởi slider
  } else {
    digitalWrite(LED_PIN, LOW);   // Đèn tắt
  }
}

// Hàm cập nhật trạng thái của nút nhấn áo tương ứng với trạng thái của đèn
void blynk_update() {
  if (update_blynk_status) {
    Blynk.virtualWrite(LED_BUTTON_DATASTREAM, led);  // Cập nhật trạng thái của nút nhấn áo trên Blynk
    update_blynk_status = 0;  // Đã cập nhật xong, chuyển về 0
  }
}
