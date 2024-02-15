/*
  Copyright (c) 2023 Daniel Zwirner
  SPDX-License-Identifier: MIT-0
*/

#include <stdio.h>
#include "pico/stdlib.h"
#include "cilo72/hw/blink_forever.h"
#include "cilo72/hw/spi_bus.h"
#include "cilo72/hw/spi_device.h"
#include "cilo72/hw/gpio.h"
#include "cilo72/hw/gpiokey.h"
#include "cilo72/hw/adc.h"
#include "cilo72/ic/mcp2515.h"
#include "cilo72/core/statemachine.h"
#include "cilo72/ic/st7735s.h"
#include "cilo72/ic/tmc5160.h"
#include "trafo.h"
#include "controlknob.h"
#include "display.h"

/*
 1 : UART0 TX/I2C0 SDA/SPI0  RX/GP00
 2 : UART0 RX/I2C0 SCL/SPI0  CS/GP01
 3 :                             GND
 4 :          I2C1 SDA/SPI0 SCK/GP02    <- Display user key 2
 5 :          I2C1 SCL/SPI0  TX/GP03    <- Display user key 3
 6 : UART1 TX/I2C0 SDA/SPI0  RX/GP04    <- TMC5160 / MCP2515 SPI RX
 7 : UART1 RX/I2C0 SCL/SPI0  CS/GP05    -> TMC5160           SPI CS
 8 :                             GND
 9 :          I2C1 SDA/SPI0 SCK/GP06    -> TMC5160 / MCP2515 SPI SCK
10 :          I2C1 SCL/SPI0  TX/GP07    -> TMC5160 / MCP2515 SPI TX
11 : UART1 TX/I2C0 SDA/SPI1  RX/GP08    -> Display data/command
12 : UART1 RX/I2C0 SCL/SPI1  CS/GP09    -> Display chip select
13 :                             GND
14 :          I2C1 SDA/SPI1 SCK/GP10    -> Display SCK
15 :          I2C1 SCL/SPI1  TX/GP11    -> Display TX
16 : UART0 TX/I2C0 SDA/SPI1  RX/GP12    -> Display reset
17 : UART0 RX/I2C0 SCL/SPI1  CS/GP13    -> Display backlight
18 :                             GND
19 :          I2C1 SDA/SPI1 SCK/GP14    -> MCP2515           SPI CS
20 :          I2C1 SCL/SPI1  TX/GP15    <- Display user key 0

21 : UART0 TX/I2C0 SDA/SPI0  RX/GP16
22 : UART0 RX/I2C0 SCL/SPI0  CS/GP17    <- Display user key 1
23 :                             GND
24 :          I2C1 SDA/SPI0 SCK/GP18    -> TMC5160 Enable
25 :          I2C1 SCL/SPI0  TX/GP19    -> FET PMW
26 :          I2C0 SDA/        /GP20    -> Relay
27 :          I2C0 SCL/        /GP21    ->
28 :                             GND
29 :                            GP22    -> CTRL Pin of 15V DC/DC converter
30 :                             RUN
31 :          I2C1 SDA/ADC0    /GP26    <- DC/DC Voltage
32 :          I2C1 SCL/ADC1    /GP27
33 :                             GND
34 :                  ADC2     /GP28
35 :                  ADC_VREF
36 :                         3V3_OUT    -> 3V3
37 :                          3V3_EN
38 :                             GND
39 :                            VSYS    <- 5V
40 :                            VBUS
*/

static constexpr uint8_t PIN_USER_2            =  2;
static constexpr uint8_t PIN_USER_3            =  3;
static constexpr uint8_t PIN_SPI1_RX           =  4;
static constexpr uint8_t PIN_SPI1_TMC5160_CS   =  5;
static constexpr uint8_t PIN_SPI1_SCK          =  6;
static constexpr uint8_t PIN_SPI1_TX           =  7;
static constexpr uint8_t PIN_ST7735S_DC        =  8;
static constexpr uint8_t PIN_ST7735S_CS        =  9;
static constexpr uint8_t PIN_SPI0_SCK          = 10;
static constexpr uint8_t PIN_SPI0_TX           = 11;
static constexpr uint8_t PIN_ST7735S_RESET     = 12;
static constexpr uint8_t PIN_ST7735S_BACKLIGHT = 13;
static constexpr uint8_t PIN_SPI1_MC2515_CS    = 14;
static constexpr uint8_t PIN_USER_0            = 15;
static constexpr uint8_t PIN_USER_1            = 17;
static constexpr uint8_t PIN_TMC5160_ENABLE    = 18;
static constexpr uint8_t PIN_FET_PWM           = 19;
static constexpr uint8_t PIN_RELAY             = 20;
static constexpr uint8_t PIN_DCDC_CTRL         = 22;
static constexpr uint8_t PIN_ADC0_15V          = 26;

static constexpr uint32_t CAN_ID_ENTER_AUTO    = 0x1000;
static constexpr uint32_t CAN_ID_LEAVE_AUTO    = 0x1001;
static constexpr uint32_t CAN_ID_SET           = 0x1002;

static constexpr uint32_t SHORT_TIME_MS        = 50;
static constexpr uint32_t SHORT_VOLTAGE_MV     = 2000;

int main()
{
  stdio_init_all();

  cilo72::hw::BlinkForever    blink(PICO_DEFAULT_LED_PIN, 1);
  cilo72::hw::Gpio            pinRelay(PIN_RELAY, cilo72::hw::Gpio::Direction::Output, cilo72::hw::Gpio::Level::Low);
  cilo72::hw::Gpio            pinDcDcCtrl(PIN_DCDC_CTRL, cilo72::hw::Gpio::Direction::Output, cilo72::hw::Gpio::Level::Low);
  cilo72::hw::Pwm             pinPwm(PIN_FET_PWM);
  cilo72::hw::SPIBus          spiBus1(PIN_SPI1_SCK, PIN_SPI1_RX, PIN_SPI1_TX);
  cilo72::hw::SPIDevice       spiTMC5160(spiBus1, PIN_SPI1_TMC5160_CS);
  cilo72::ic::Tmc5160         tmc5160(spiTMC5160, PIN_TMC5160_ENABLE);
  cilo72::hw::SPIDevice       spiMCP2515(spiBus1, PIN_SPI1_MC2515_CS);
  cilo72::ic::MCP2515         can(spiMCP2515, cilo72::ic::MCP2515::Oscillator::F_16MHZ, cilo72::ic::MCP2515::Bitrate::B_250KBPS);
  cilo72::hw::SPIBus          spiBus0(PIN_SPI0_SCK, PIN_SPI0_TX);
  cilo72::hw::SPIDevice       spiSt7735S(spiBus0, PIN_ST7735S_CS);
  cilo72::graphic::FramebufferRGB565 fb(128, 128);
  cilo72::ic::ST7735S         st7735S(fb, spiSt7735S, PIN_ST7735S_DC, PIN_ST7735S_RESET, PIN_ST7735S_BACKLIGHT);
  cilo72::hw::Adc             adc0(PIN_ADC0_15V);
  cilo72::hw::GpioKey         keyUser1(PIN_USER_0, cilo72::hw::Gpio::Pull::Up);
  Trafo                       trafo(pinPwm, pinRelay);
  ControlKnob                 controlKnob(tmc5160);
  Display                     display(st7735S);
  cilo72::core::CanMessage    rxMessage;
  cilo72::ic::MCP2515::Error  errorCan = cilo72::ic::MCP2515::Error::OK;
  cilo72::core::State         stateManuel;
  cilo72::core::State         stateAuto;
  cilo72::core::State         stateShort;
  cilo72::hw::ElapsedTimer_ms timer;

  static const cilo72::graphic::Color COLOR_MANUAL_BG = cilo72::graphic::Color(153, 255, 51);
  static const cilo72::graphic::Color COLOR_MANUAL_FG = cilo72::graphic::Color::black;
  
  static const cilo72::graphic::Color COLOR_AUTO_BG   = cilo72::graphic::Color::blue;
  static const cilo72::graphic::Color COLOR_AUTO_FG   = cilo72::graphic::Color::white;
  
  can.reset();

  stateAuto.setOnEnter([&]()
  {
    pinDcDcCtrl.set();
    trafo.off();
    controlKnob.off();
    display.draw(COLOR_AUTO_BG, COLOR_AUTO_FG, trafo.power()); 
  });

  stateAuto.setOnRun([&](cilo72::core::State &state) -> const cilo72::core::StateMachineCommand *
  {
    if(keyUser1.pressed())
    {
      return state.changeTo(&stateManuel);
    }

    if(errorCan == cilo72::ic::MCP2515::Error::OK)
    {
      if(rxMessage.id() == CAN_ID_LEAVE_AUTO)
      {
        return state.changeTo(&stateManuel);
      }
      else if(rxMessage.id() == CAN_ID_SET and rxMessage.dlc() >= 2)
      {
        int32_t p = rxMessage[0];
        if(rxMessage[1] == 0) 
        {
          p = -p;
        }

        trafo.setPower(p);
        display.draw(COLOR_AUTO_BG, COLOR_AUTO_FG, trafo.power());
      }
    }

    return state.nothing();
  });

  stateShort.setOnEnter([&]()
  {
    pinDcDcCtrl.clear();
    trafo.off();
    controlKnob.off();
    display.drawShort(); 
  });

  stateShort.setOnRun([&](cilo72::core::State &state) -> const cilo72::core::StateMachineCommand *
  {
    if(keyUser1.pressed())
    {
      return state.changeTo(&stateManuel);
    }
    else
    {
      return state.nothing();
    }
  });

  stateManuel.setOnEnter([&]()
  {
    pinDcDcCtrl.set();
    trafo.off();
    controlKnob.off();
    display.draw(COLOR_MANUAL_BG, COLOR_MANUAL_FG, controlKnob.position());
  });

  stateManuel.setOnRun([&](cilo72::core::State &state) -> const cilo72::core::StateMachineCommand *
  {
    if(keyUser1.pressed())
    {
      trafo.off();
      controlKnob.off();      
    }
    controlKnob.run();
    if(controlKnob.hasChanged())
    {
      trafo.setPower(controlKnob.position());
      display.draw(COLOR_MANUAL_BG, COLOR_MANUAL_FG, controlKnob.position());
    }

    if(errorCan == cilo72::ic::MCP2515::Error::OK)
    {
      if(rxMessage.id() == CAN_ID_ENTER_AUTO)
      {
        return state.changeTo(&stateAuto);
      }      
    }

    return state.nothing();
  });

  cilo72::core::StateMachine sm(&stateManuel);

  while (true)
  {
    errorCan = can.readMessage(rxMessage);

    sm.run();

    if (adc0.read() < SHORT_VOLTAGE_MV and pinDcDcCtrl.isHigh() == true)
    {
      if(timer.isValid() == false)
      {
        timer.start();
      }
      else if(timer.hasExpired(SHORT_TIME_MS))
      {
        sm.changeStateIfOther(&stateShort);
      }
    }
    else
    {
      timer.invalidate();
    }
  }
  return 0;
}
