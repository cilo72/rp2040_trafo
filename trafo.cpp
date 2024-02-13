#include "trafo.h"

  Trafo::Trafo(cilo72::hw::Pwm &pwm, cilo72::hw::Gpio &relay)
  : pwm_(pwm)
  , relay_(relay)
  , power_(0)
  {
    pwm_.enable();
    pwm_.setFrequency(10000);
    off();
  }

  void Trafo::off()
  {
    setPower(0);
  }

  void Trafo::setPower(int32_t power)
  {
    if(power == 0)
    {
      pwm_.setDutyCycleU32(0);
      relay_.clear();
    }
    else if(power > 0)
    {
      pwm_.setDutyCycleU32(power);
      relay_.clear();
    }
    else
    {
      pwm_.setDutyCycleU32(-power);
      relay_.set();
    }
    power_ = power;
  }

  int32_t Trafo::power() const
  {
    return power_;
  }

