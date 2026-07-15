/*
 Todo: implement position filter
*/

#ifndef SPI_ENCODER_HPP
#define SPI_ENCODER_HPP

#include "encoder.hpp"




const uint16_t EncoderReadCmd = (0b11 << 14) | 0x3FFF;

class SPIEncoder : public AbsoluteEncoder
{
public:
  SPIEncoder() = default;
  ~SPIEncoder() = default;
  SPIEncoder(
    const uint16_t read_cmd,
    SPIClass & spi,
    const int chip_select,
    const int speed = 10000000,
    const int bit_order = MSBFIRST,
    const int mode = SPI_MODE1,
    const int res = 14
  )
  : read_cmd_(read_cmd),
    SPI_(spi),
    settings_(SPISettings(speed, bit_order, mode)),
    cs_(chip_select),
    max_read_((1 << res))
  {
    pinMode(cs_, OUTPUT);
    digitalWriteFast(cs_, HIGH);
    SPI_.begin();
    //SPI_.beginTransaction(settings_);
  }

  /// @brief read the encoder with continuous unwrapping
  /// @returns the unwrapped angle (no discontinuity at the wrap boundary)
  float read()
  {
    uint16_t r = read_raw();
    if (r == 0 || r == 16383) {
      return angle_;  // error: return last valid unwrapped angle
    }
    float raw = static_cast<float>(r) / static_cast<float>(max_read_ - 1)
                * static_cast<float>(M_2_PI);
    if (initialized_) {
      float delta = raw - prev_raw_;
      if (delta > static_cast<float>(M_2_PI) * 0.5f) {
        wrap_count_--;
      } else if (delta < -static_cast<float>(M_2_PI) * 0.5f) {
        wrap_count_++;
      }
    } else {
      initialized_ = true;
    }
    prev_raw_ = raw;
    angle_ = raw + static_cast<float>(wrap_count_) * static_cast<float>(M_2_PI);
    return angle_;
  }

  uint16_t read_raw()
  {
    SPI_.beginTransaction(settings_);
    digitalWriteFast(cs_, LOW);
    SPI_.transfer16(read_cmd_);
    digitalWriteFast(cs_, HIGH);
    delayNanoseconds(400);
    digitalWriteFast(cs_, LOW);
    uint16_t raw = SPI_.transfer16(0x000);
    digitalWriteFast(cs_, HIGH);
    SPI_.endTransaction();
    return ((raw >> 0) << 0) & (max_read_ - 1);

  }

  void zero_position(){
    digitalWriteFast(cs_, LOW);
    SPI_.transfer16(read_cmd_);
    digitalWriteFast(cs_, HIGH);
    delayNanoseconds(400);
    digitalWriteFast(cs_, LOW);
    uint16_t raw = SPI_.transfer16(0x000);
    digitalWriteFast(cs_, HIGH);
    return ((raw >> 0) << 0) & (max_read_ - 1);
  }

private:
  SPIClass & SPI_;
  const SPISettings settings_;
  const int cs_;
  const uint16_t read_cmd_;
  const int max_read_;

  // Unwrap state
  float prev_raw_ = 0.0f;
  int32_t wrap_count_ = 0;
  bool initialized_ = false;
  float angle_ = 0.0f;
};
#endif
