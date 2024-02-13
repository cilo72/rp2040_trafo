#include "controlknob.h"

static bool inRange(int32_t value, int32_t min, int32_t max)
{
  return value >= min && value <= max;
}

  ControlKnob::ControlKnob(cilo72::ic::Tmc5160 &tmc)
  : tmc_(tmc)
  , changed_(false)
  , position_(0)
  , powerAfter_(0)
  , powerPulseActive_(false)
  , inZeroNotch_(false)
  , lastInZeroNotch_(false)
  , state_(State::Undefined)
  , lastState_(State::Undefined)
  {
    cilo72::ic::Tmc5160::Gconf gconf;
    tmc_.gconf(gconf);
    gconf.en_pwm_mode = 1;
    tmc_.setGconf(gconf);
    tmc_.setTpwmThrs(1048575-1);
    tmc.setTcoolThrs(1048575-1);

    cilo72::ic::Tmc5160::GlobalScaler globalScaler = {0};
    globalScaler.globalScaler = 32;
    tmc_.setGlobalScaler(globalScaler);

    tmc_.setEncoderFactor(1.0);

    init();
  }

  void ControlKnob::off()
  {
    init();
  }

  void ControlKnob::init()
  {
    setPower(10);
    sleep_us(100000);
    tmc_.setXEnc(0);
    tmc_.xEnc(encoder_);
    lastEncoder_ = encoder_;
    inZeroNotch_ = lastInZeroNotch_ = true;
    state_ = lastState_ = State::ZeroNotch;
    position_ = 0;
  }

  void ControlKnob::run()
  {
    tmc_.xEnc(encoder_);
    if(encoder_ != lastEncoder_)
    {
      lastEncoder_ = encoder_;
      changed_ = true;
    }

    switch(state_)
    {
        case State::Undefined:
        {
          init();
        }
        break;
      
        case State::ZeroNotch:
        {
            if(not inRange(encoder_, -(NULL_OFFSET + HYSTERESE), (NULL_OFFSET + HYSTERESE)))
            {
                setPower(0);
                state_ = State::OutsideNotch;
            }
        }
        break;

        case State::OutsideNotch:
        {
            if(inRange(encoder_, -NULL_OFFSET, NULL_OFFSET))
            {
                position_ = 0;
                if(lastState_ != State::ZeroNotch)
                {
                    currentPulse(20, 2);
                    sleep_us(100);
                    tmc_.setXEnc(0);
                }
                state_ = State::ZeroNotch;
            }
            else if(encoder_ > MAX_ENCODER_VALUE)
            {
                position_ = 100;
                if(lastState_ != State::Max)
                {
                    currentPulse(20, 2);
                }
                state_ = State::Max;
            }
            else if(encoder_ < -MAX_ENCODER_VALUE)
            {
                position_ = -100;
                if(lastState_ != State::Min)
                {
                    currentPulse(20, 2);
                }
                state_ = State::Min;
            }            
            else
            {
                if(inRange(encoder_, NULL_OFFSET, MAX_ENCODER_VALUE))
                {
                    position_ = ((encoder_ - NULL_OFFSET) * 100) / (MAX_ENCODER_VALUE - NULL_OFFSET);
                }
                else if(inRange(encoder_, -MAX_ENCODER_VALUE, -NULL_OFFSET))
                {
                    position_ = ((encoder_ + NULL_OFFSET) * 100) / (MAX_ENCODER_VALUE - NULL_OFFSET);
                }
            }
        }
        break;

        case State::Max:
        {
            if(encoder_ < (MAX_ENCODER_VALUE - HYSTERESE))
            {
                setPower(0);
                state_ = State::OutsideNotch;
            }
            else if(encoder_ > MAX_ENCODER_VALUE + HYSTERESE)
            {
                currentPulse(20, 2);
                tmc_.setXEnc(MAX_ENCODER_VALUE + HYSTERESE);
            }
        }
        break;

        case State::Min:
        {
            if(encoder_ > -(MAX_ENCODER_VALUE - HYSTERESE))
            {
                setPower(0);
                state_ = State::OutsideNotch;
            }
            else if(encoder_ < -(MAX_ENCODER_VALUE + HYSTERESE))
            {
                currentPulse(20, 2);
                tmc_.setXEnc(-(MAX_ENCODER_VALUE + HYSTERESE));
            }
        }
        break;
    }

    lastState_ = state_;



    if(powerPulseActive_)
    {
      if(timer_.hasExpired(300))
      {
        setPower(powerAfter_);
        powerPulseActive_ = false;
      }
    }

    lastInZeroNotch_ = inZeroNotch_;
  }

  void ControlKnob::currentPulse(uint32_t pulse, uint32_t after)
  {
    powerAfter_ = after;
    setPower(pulse);
    timer_.start();
    powerPulseActive_ = true;
  }

  bool ControlKnob::hasChanged()
  {
    bool ret = changed_;
    changed_ = false; 
    return ret;
  }

  void ControlKnob::setPower(uint32_t power) const
  {
    cilo72::ic::Tmc5160::ChopConf chopconf;
    tmc_.chopConf(chopconf);
    chopconf.mres = 0;
    chopconf.toff = power == 0 ? 0 : 3;
    chopconf.hstrt = 4;
    chopconf.hend = 1;
    chopconf.tbl = 2;
    chopconf.chm = 0;
    tmc_.setChopConf(chopconf);

    if(power > 0)
    {
      if(power > 31)
      {
        power = 31;
      }
      cilo72::ic::Tmc5160::IHold_IRun ihold_irun = {0};
      ihold_irun.ihold = power;
      ihold_irun.irun = 0;
      tmc_.setIholdIRun(ihold_irun);
    }
  }

  int32_t ControlKnob::position() const
  {
    return position_;
  }

  int32_t ControlKnob::encoder() const
  {
    return encoder_;
  }
