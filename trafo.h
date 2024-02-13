#include "cilo72/hw/gpio.h"
#include "cilo72/hw/pwm.h"

class Trafo
{
  public:
  Trafo(cilo72::hw::Pwm &pwm, cilo72::hw::Gpio &relay);
  
  void off();
  
  void setPower(int32_t power);

  int32_t power() const;
  private:
    cilo72::hw::Pwm &pwm_;
    cilo72::hw::Gpio &relay_;
    int32_t power_;
};
