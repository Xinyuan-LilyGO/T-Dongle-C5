/* This example shows how to make an LED pattern with a large
 * dynamic range using the the extra 5-bit brightness register in
 * the APA102.
 *
 * It sets every LED on the strip to white, with the dimmest
 * possible white at the input end of the strip and the brightest
 * possible white at the other end, and a smooth logarithmic
 * gradient between them.
 *
 * The dimmest possible white is achieved by setting the red,
 * green, and blue color channels to 1, and setting the
 * brightness register to 1.  The brightest possibe white is
 * achieved by setting the color channels to 255 and setting the
 * brightness register to 31.
 */

/* By default, the APA102 library uses pinMode and digitalWrite
 * to write to the LEDs, which works on all Arduino-compatible
 * boards but might be slow.  If you have a board supported by
 * the FastGPIO library and want faster LED updates, then install
 * the FastGPIO library and uncomment the next two lines: */
// #include <FastGPIO.h>
// #define APA102_USE_FAST_GPIO

#include <APA102.h>

// Define which pins to use.
const uint8_t dataPin = 5;
const uint8_t clockPin = 4;

// Create an object for writing to the LED strip.
APA102<dataPin, clockPin> ledStrip;

// Set the number of LEDs to control.
const uint16_t ledCount = 1;

// We define "power" in this sketch to be the product of the
// 8-bit color channel value and the 5-bit brightness register.
// The maximum possible power is 255 * 31 (7905).
const uint16_t maxPower = 255 * 31;

// The power we want to use on the first LED is 1, which
// corresponds to the dimmest possible white.
const uint16_t minPower = 1;

// Calculate what the ratio between the powers of consecutive
// LEDs needs to be in order to reach the max power on the last
// LED of the strip.
const float multiplier = pow(maxPower / minPower, 1.0 / (ledCount - 1));

void setup()
{
    pinMode(dataPin, OUTPUT);
    pinMode(clockPin, OUTPUT);

    Serial.begin(115200);
    delay(1000); // power-up safety delay
    Serial.println();
    Serial.println("Starting LED strip example...");
}

void loop()
{
  // 使用 millis() 获取时间，用于生成周期性的变化
  unsigned long time = millis();
  
  // 计算颜色渐变：使用正弦函数生成 0 到 255 之间的值
  // 不同的相位偏移量 (0, 2, 4) 产生 RGB 三种颜色的不同变化
  // 周期设为 3000ms，使颜色变化速度适中，不刺眼
  // 使用 pow(x, 2.2) 进行伽马校正，使颜色过渡更柔和
  float rawRed = (sin(time / 3000.0 * 2 * PI) + 1) / 2;
  float rawGreen = (sin(time / 3000.0 * 2 * PI + 2) + 1) / 2;
  float rawBlue = (sin(time / 3000.0 * 2 * PI + 4) + 1) / 2;

  uint8_t red = (uint8_t)(pow(rawRed, 2.2) * 255);
  uint8_t green = (uint8_t)(pow(rawGreen, 2.2) * 255);
  uint8_t blue = (uint8_t)(pow(rawBlue, 2.2) * 255);

  ledStrip.startFrame();
  // 固定亮度为最大值，完全由 RGB 值控制颜色
  // 将 5位亮度设为 10 (约32%亮度)，避免过亮刺眼
  ledStrip.sendColor(red, green, blue, 10);
  ledStrip.endFrame(ledCount);
  
  // 短暂延时以控制刷新率
  delay(8);
}

