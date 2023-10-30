/*************************************************************
  Blynk is a platform with iOS and Android apps to control
  ESP32, Arduino, Raspberry Pi and the likes over the Internet.
  You can easily build mobile and web interfaces for any
  projects by simply dragging and dropping widgets.

    Downloads, docs, tutorials: https://www.blynk.io
    Sketch generator:           https://examples.blynk.cc
    Blynk community:            https://community.blynk.cc
    Follow us:                  https://www.fb.com/blynkapp
                                https://twitter.com/blynk_app

  Blynk library is licensed under MIT license
 *************************************************************
  Blynk.Edgent implements:
  - Blynk.Inject - Dynamic WiFi credentials provisioning
  - Blynk.Air    - Over The Air firmware updates
  - Device state indication using a physical LED
  - Credentials reset using a physical Button
 *************************************************************/

/* Fill in information from your Blynk Template here */
/* Read more: https://bit.ly/BlynkInject */
#define BLYNK_TEMPLATE_ID "TMPL6lwrbAEGk"
#define BLYNK_TEMPLATE_NAME "LedControl2"

#define BLYNK_FIRMWARE_VERSION "0.1.0"

#define BLYNK_PRINT Serial
//#define BLYNK_DEBUG

#define APP_DEBUG

// Uncomment your board, or configure a custom board in Settings.h
//#define USE_SPARKFUN_BLYNK_BOARD
#define USE_NODE_MCU_BOARD
//#define USE_WITTY_CLOUD_BOARD
//#define USE_WEMOS_D1_MINI

#define LED_PIN D0  // Pin for the LED

#define LED_CONTROL_PIN V0  // Widget for LED control
#define LED_TIMER V2        // Widget for LED timer

#include "BlynkEdgent.h"
#include <TimeLib.h>

BlynkTimer timer;

bool led;
long timer_start = 0xFFFF; // thời điểm đèn bắt đầu sáng (tính bằng giây)
long timer_stop = 0xFFFF;   // thời điểm đèn ngừng sáng (tính bằng giây)
unsigned char weekday_option;

long rtc_sec; // thời điểm hiện tại (tính bằng giây)
unsigned char day_of_week;

bool led_status; // trạng thái của led và nút nhấn ảo (0: tắt, 1: bật)
bool update_blynk_status;
bool led_timer_on_set;

// #########################################################################################################
// LED 1
BLYNK_WRITE(LED_CONTROL_PIN) {
  int val = param.asInt();
  // Nếu không trong thời gian của timer, có thể dùng nút nhấn để điều khiển led
  if (led_timer_on_set == 0)
    led = val; 
  // khi đang trong thời gian của timer thì không thể nhấn nút ảo để điều khiển led
  else
    update_blynk_status = 1; // cập nhật trạng thái của led cho nút ảo
}

// #########################################################################################################
// Timer 1
BLYNK_WRITE(LED_TIMER) {
  unsigned char week_day;

  TimeInputParam t(param);

  if (t.hasStartTime() && t.hasStopTime()) {
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
BLYNK_WRITE(InternalPinRTC) {
  const unsigned long DEFAULT_TIME = 1357041600;  // Mốc thời gian mặc định 1/1/2013
  unsigned long blynkTime = param.asLong();

  if (blynkTime >= DEFAULT_TIME) {
    setTime(blynkTime);

    day_of_week = weekday();

    if (day_of_week == 1)
      day_of_week = 7;
    else
      day_of_week -= 1;

    rtc_sec = (hour() * 60 * 60) + (minute() * 60) + second();

    Serial.println(blynkTime);
    Serial.println(String("RTC Server: ") + hour() + ":" + minute() + ":" + second());
    Serial.println(String("Day of Week: ") + weekday());
  }
}

// #########################################################################################################
BLYNK_CONNECTED() {
  Blynk.sendInternal("rtc", "sync");
}

// #########################################################################################################
void checkTime() {
  Blynk.sendInternal("rtc", "sync");
}

// #########################################################################################################
void led_mng() {
  bool old_led_status;
  old_led_status = led_status;


  if (timer_start != 0xFFFF && timer_stop != 0xFFFF) {

    if (
      (
        (
          ((timer_start <= timer_stop) && (rtc_sec >= timer_start) && (rtc_sec < timer_stop)) 
          || ((timer_start >= timer_stop) && ((rtc_sec >= timer_start) || (rtc_sec < timer_stop)))
        ) && (weekday_option == 0x00 || (weekday_option & (0x01 << (day_of_week - 1)))))) 
    {
      led_timer_on_set = 1;
    } else
      led_timer_on_set = 0;
  } else
    led_timer_on_set = 0;

  if (led_timer_on_set) {
    led_status = 1;
    led = 0;
  } else {
    led_status = led;
  }

  if (old_led_status != led_status)
    update_blynk_status = 1;

  // HARDWARE CONTROL
  digitalWrite(LED_PIN, led_status); // thay đổi trạng thái Led
}

// cập nhật trạng thái nút nhấn áo tương ứng với trạng thái của led
void blynk_update() {
  if (update_blynk_status) {
    Blynk.virtualWrite(LED_CONTROL_PIN, led_status); // thay đổi trạng thái nút nhấn ảo
    update_blynk_status = 0; // đã update xong, chuyển về 0
  }
}

// #########################################################################################################
void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(LED_PIN, OUTPUT);


  BlynkEdgent.begin();
  timer.setInterval(10000L, checkTime);
}

// #########################################################################################################
void loop() {
  BlynkEdgent.run();
  timer.run();
  led_mng();
  blynk_update();
}