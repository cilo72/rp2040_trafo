#include "cilo72/ic/tmc5160.h"
#include "cilo72/hw/elapsed_timer_ms.h"

class ControlKnob
{
public:
    ControlKnob(cilo72::ic::Tmc5160 &tmc);

    void init();

    void off();

    void run();

    void currentPulse(uint32_t pulse, uint32_t after);

    bool hasChanged();

    void setPower(uint32_t power) const;

    int32_t position() const;

    int32_t encoder() const;

private:
enum class State
{
    Undefined,
    ZeroNotch,
    OutsideNotch,
    Max,
    Min
};

    cilo72::ic::Tmc5160 &tmc_;
    int32_t encoder_;
    int32_t lastEncoder_;
    bool changed_;
    int32_t position_;
    cilo72::hw::ElapsedTimer_ms timer_;
    uint32_t powerAfter_;
    bool powerPulseActive_;
    bool inZeroNotch_;
    bool lastInZeroNotch_;
    State state_;
    State lastState_;
    static constexpr int32_t MAX_ENCODER_VALUE = 3100;
    static constexpr int32_t NULL_OFFSET = 50;
    static constexpr int32_t HYSTERESE = 50;
};