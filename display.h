#include "cilo72/ic/st7735s.h"

class Display
{
    public:
        Display(const cilo72::ic::ST7735S & display);
        void draw(cilo72::graphic::Color bg, int32_t power);
        void drawShort();
    private:
        const cilo72::ic::ST7735S & display_;
};