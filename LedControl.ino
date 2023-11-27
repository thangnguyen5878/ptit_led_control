// Định nghĩa template ID, tên và phiên bản firmware của Blynk
#define BLYNK_TEMPLATE_ID "TMPL6lwrbAEGk"
#define BLYNK_TEMPLATE_NAME "LedControl2"
#define BLYNK_FIRMWARE_VERSION "0.1.0"

#define BLYNK_PRINT Serial
#define APP_DEBUG

// Chọn board sử dụng
#define USE_NODE_MCU_BOARD

// Định nghĩa các chân cho LED
#define LED1_PIN D0  // Chân cho LED 1
#define LED2_PIN D2 // Chân cho LED 2
#define LED3_PIN D5 // Chân cho LED 3


// Định nghĩa các chân cho cảm biến
#define LIGHT_SENSOR_PIN A0 // Chân cho cảm biến ánh sáng
#define PIR_SENSOR_PIN D1 // Chân cho cảm biến chuyển động (D1)

// Định nghĩa các datastream trên Blynk
#define LED_BUTTON_DATASTREAM V0  // Button Widget (Integer): 0-1
#define LED_SLIDER_DATASTREAM V1  // Slider Widget (Integer): 1-100
#define LED_TIMER_DATASTREAM V2   // Timer Widget (String)

#define LIGHT_SENSOR_DATASTREAM V3   // Widget cho cảm biến chuyển động (Integer): 0-100
#define PIR_SENSOR_DATASTREAM V4  // Widget cho cảm biến ánh sáng (Integer): 0-1

#define LED1_DATASTREAM V5  // LED 1 Widget (Integer): 0-1
#define LED2_DATASTREAM V6  // LED 2 Widget (Integer): 0-1
#define LED3_DATASTREAM V7  // LED 3 Widget (Integer): 0-1

// Khai báo thư viện
#include <ESP8266WiFi.h>
#include "BlynkEdgent.h"
#include <TimeLib.h>


BlynkTimer timer;
int slider;  // Giá trị của slider để điều chỉnh độ sáng LED1
bool button;  // Giá trị của nút nhấn ảo (0: tắt, 1: bật)

// Biến cho Timer
long timer_start = 0xFFFF;  // Thời điểm đèn bắt đầu sáng (tính bằng giây)
long timer_stop = 0xFFFF;   // Thời điểm đèn ngừng sáng (tính bằng giây)
unsigned char weekday_option; // Lựa chọn ngày trong tuần

int lightSensor; // Giá trị của cảm biến ánh sáng
int lightSensorPercentage; // Phần trăm giá trị của cảm biến ánh sáng (%)

int pirSensor; // Giá trị của cảm biến chuyển động

long rtc_sec;  // Thời điểm hiện tại (tính bằng giây)
unsigned char day_of_week;

bool led1;                  // Trạng thái của LED (0: tắt, 1: bật)

// Biến quản lý cập nhật Blynk
// update_blynk_status = 1: cập nhật trạng thái của LED cho nút nhấn ảo
bool update_blynk_status;  

// Biến kiểm soát Timer
// is_timer_on = 1: đèn sáng theo cài đặt của Timer
bool is_timer_on;          

void setup() {
  Serial.begin(115200);
  delay(100);

  // Đặt chế độ chân cho LED và cảm biến
  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);
  pinMode(LED3_PIN, OUTPUT);
  pinMode(LIGHT_SENSOR_PIN, INPUT);
  pinMode(PIR_SENSOR_PIN, INPUT);

  // Tắt tất cả LED
  digitalWrite(LED1_PIN, LOW);
  digitalWrite(LED2_PIN, LOW);
  digitalWrite(LED3_PIN, LOW);

  // Khởi tạo Blynk và Timer
  BlynkEdgent.begin();
  timer.setInterval(10000L, checkTime);
}


void loop() {
    // Duy trì kết nối và quản lý trạng thái của thiết bị khi sử dụng chế độ "Edgent" của Blynk
    BlynkEdgent.run();

    // 1. Chức năng cơ bản: Điều khiển LED 1 bằng nút nhấn ảo, slide và timer
    timer.run();        // Chạy bộ đếm thời gian BlynkTimer để kiểm tra và cập nhật thời gian
    led_mng();          // Quản lý trạng thái của LED
    blynk_update();     // Cập nhật trạng thái của LED 1 cho nút nhấn ảo trong ứng dụng Blynk


    // 2. Chức năng với cảm biến ánh sáng
    // Đọc giá trị của cảm biến ánh sáng
  lightSensor = analogRead(LIGHT_SENSOR_PIN);  
  // Chuyển thành đơn vị phần trăm
  lightSensorPercentage = map(lightSensor, 0, 1023, 100, 0);
    // Cập nhật LIGHT_SENSOR_DATASTREAM trên app Blynk
  Blynk.virtualWrite(LIGHT_SENSOR_DATASTREAM, lightSensorPercentage);
  // Nếu giá trị của cảm biến ánh sáng từ 20 trở xuống, bật LED 2. Nếu không thì tắt LED 2
  if (lightSensorPercentage <= 20) {
    digitalWrite(LED2_PIN, HIGH);  
    Blynk.virtualWrite(LED2_DATASTREAM, HIGH);
  } else {
    digitalWrite(LED2_PIN, LOW);  
    Blynk.virtualWrite(LED2_DATASTREAM, LOW); 
  }

  // 3. Chức năng với cảm biến chuyển động 
  // Đọc giá trị của cảm biến chuyển động
  pirSensor = digitalRead(PIR_SENSOR_PIN);
  // Nếu phát hiện chuyển động, bật LED 3 và cập nhật PIR_SENSOR_DATASTREAM trên app Blynk
  if(pirSensor == HIGH) {
    digitalWrite(LED3_PIN, HIGH);  
    Blynk.virtualWrite(PIR_SENSOR_DATASTREAM, HIGH);
    Blynk.virtualWrite(LED3_DATASTREAM, HIGH);
    delay(1000); // Delay 1 giây
  } else {
    // Nếu không phát hiện chuyển động, tắt LED 3 và cập nhật PIR_SENSOR_DATASTREAM trên app Blynk
    digitalWrite(LED3_PIN, LOW);   
    Blynk.virtualWrite(PIR_SENSOR_DATASTREAM, LOW);
    Blynk.virtualWrite(LED3_DATASTREAM, LOW);
  }  
}

// Cập nhật giá trị của nút nhấn ảo (0-1) điều khiển LED 1
BLYNK_WRITE(LED_BUTTON_DATASTREAM) {
  int val = param.asInt();
  // Nếu không trong thời gian của timer, có thể dùng nút nhấn ảo để điều khiển LED 1
  if (is_timer_on == 0)
    button = val;
  else
  // khi đang trong thời gian của timer thì không thể nhấn nút nhấn ảo để điều khiển LED 1
    update_blynk_status = 1;  // Cập nhật trạng thái của LED 1 cho nút nhấn ảo
}

// Cập nhật giá trị của Slider (1-100) điều khiển LED 2
BLYNK_WRITE(LED_SLIDER_DATASTREAM) {
  // Đọc giá trị của slider trên app Blynk
  slider = param.asInt();
  Serial.print(slider);
  // Nếu LED đang sáng thì mới có thể thay đổi độ sáng
  if (button == 1)
    analogWrite(LED1_PIN, slider);
}

// Cập nhật giá trị của Timer điều khiển LED 1
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

// Callback được gọi khi thiết bị kết nối với server Blynk
BLYNK_CONNECTED() {
  // Gửi lệnh "rtc sync" để đồng bộ thời gian với server
  Blynk.sendInternal("rtc", "sync");
}

// Callback được gọi trong hàm loop để đồng bộ thời gian với server Blynk
void checkTime() {
  Blynk.sendInternal("rtc", "sync");
}

// Hàm quản lý thời gian và trạng thái LED 1
void led_mng() {
    // Lưu tạm giá trị cũ của LED 1
  bool old_led_status;
  old_led_status = led1;

  // Kiểm tra xem có đang trong khoảng thời gian LED 1 sáng mà được đặt bởi timer không
  if (timer_start != 0xFFFF && timer_stop != 0xFFFF) {
    if (
      (
        (
          ((timer_start <= timer_stop) && (rtc_sec >= timer_start) && (rtc_sec < timer_stop))
          || ((timer_start >= timer_stop) && ((rtc_sec >= timer_start) || (rtc_sec < timer_stop))))
        && (weekday_option == 0x00 || (weekday_option & (0x01 << (day_of_week - 1)))))) {
      is_timer_on = 1;  
    } else {
        is_timer_on = 0;  
    }
  } else {
    is_timer_on = 0;
  }
    

  // Kiểm tra và cập nhật trạng thái của LED 1 và nút nhấn áo
  // Nếu đang trong thời gian LED 1 sáng được đặt timer, không thể điều khiển LED 1 bằng nút nhấn áo
  // Nếu không thì trạng thái của LED 1 sẽ được điều khiển bằng nút nhấn áo
  if (is_timer_on) {
    led1 = 1;       
    // button = 0;   
  } else {
    led1 = button;  
  }

  // Nếu trạng tháng của LED 1 thay đổi, cập nhật trên app Blynk
  if (old_led_status != led1)
    update_blynk_status = 1;

  // Kiểm soát trạng thái của đèn trên thiết bị
  // Nếu LED 1 sáng, điều chỉnh độ sáng của LED 1 bằng giá trị của slider
  // Nếu LED 1 tăt, giữ đèn tắt và không thể điều chỉnh độ sáng của đèn bằng slider
  if (led1 == 1) {
    analogWrite(LED1_PIN, slider); 
  } else {
    digitalWrite(LED1_PIN, LOW); 
  }
}

// Cập nhật trạng thái của LED 1 cho nút nhấn ảo
void blynk_update() {
  if (update_blynk_status) {
    // Cập nhật trạng thái của nút nhấn áo trên Blynk
    Blynk.virtualWrite(LED_BUTTON_DATASTREAM, led1); 
    Blynk.virtualWrite(LED1_DATASTREAM, led1);  
    // Đã cập nhật xong, chuyển về 0
    update_blynk_status = 0;  
  }
}
