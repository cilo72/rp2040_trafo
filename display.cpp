#include "display.h"
#include "pico/multicore.h"
#include "pico/util/queue.h"
#include <stdio.h>

namespace
{
    const cilo72::ic::ST7735S *g_display = nullptr;
    queue_t call_queue;

    enum class CallType
    {
        DrawDisplay,
        DrawShort
    };

    typedef struct
    {
        CallType callType;
        union data
        {
            struct drawDisplay
            {
                uint8_t r;
                uint8_t g;
                uint8_t b;
                int32_t power;
            } drawDisplay;
        } data;
    } queue_entry_t;

    void core1_entry()
    {
        g_display->init();

        static constexpr cilo72::fonts::Font8x5 font;
        int x = g_display->framebuffer().height() / 2;
        int y = g_display->framebuffer().height() / 2;

        queue_entry_t entry;
        while (true)
        {
            queue_remove_blocking(&call_queue, &entry);

            switch (entry.callType)
            {
            case CallType::DrawDisplay:
            {                
                char buff[22];
                sprintf(buff, "%i", abs(entry.data.drawDisplay.power));
                static constexpr int f = 4;
                g_display->framebuffer().clear(cilo72::graphic::Color(entry.data.drawDisplay.r, entry.data.drawDisplay.g, entry.data.drawDisplay.b));
                g_display->framebuffer().drawString(x, y, f, buff, cilo72::graphic::Color::white, font, cilo72::graphic::Framebuffer::Center);
                if(entry.data.drawDisplay.power > 0)
                {
                    g_display->framebuffer().drawString(g_display->framebuffer().width() - 1, g_display->framebuffer().height() / 2, f, ">" , cilo72::graphic::Color::white, font, cilo72::graphic::Framebuffer::CenterRight);
                }
                else if(entry.data.drawDisplay.power < 0)
                {
                    g_display->framebuffer().drawString(1, g_display->framebuffer().height() / 2, f, "<" , cilo72::graphic::Color::white, font, cilo72::graphic::Framebuffer::CenterLeft);
                }
                g_display->framebuffer().drawString(g_display->framebuffer().width() - 1, g_display->framebuffer().height(), 2, "Stop >" , cilo72::graphic::Color::white, font, cilo72::graphic::Framebuffer::BottomRight);
                g_display->update();
            }
            break;

            case CallType::DrawShort:
            {
                g_display->framebuffer().clear(cilo72::graphic::Color::red);
                g_display->framebuffer().drawString(x, y, 8, "!" , cilo72::graphic::Color::white, font, cilo72::graphic::Framebuffer::Center);
                g_display->framebuffer().drawString(g_display->framebuffer().width() - 1, g_display->framebuffer().height(), 2, "Stop >" , cilo72::graphic::Color::white, font, cilo72::graphic::Framebuffer::BottomRight);
                g_display->update();
            }
            break;
            }
        }
    }
}

Display::Display(const cilo72::ic::ST7735S &display)
    : display_(display)
{
    g_display = &display;
    queue_init(&call_queue, sizeof(queue_entry_t), 2);
    multicore_launch_core1(core1_entry);
}

void Display::draw(cilo72::graphic::Color bg, int32_t power)
{
    queue_entry_t entry;

    if (queue_is_full(&call_queue))
    {
        queue_try_remove(&call_queue, &entry);
    }

    entry.callType = CallType::DrawDisplay;
    entry.data.drawDisplay.r = bg.r();
    entry.data.drawDisplay.g = bg.g();
    entry.data.drawDisplay.b = bg.b();

    entry.data.drawDisplay.power = power;

    queue_try_add(&call_queue, &entry);
}

void Display::drawShort()
{
    queue_entry_t entry;

    if (queue_is_full(&call_queue))
    {
        queue_try_remove(&call_queue, &entry);
    }

    entry.callType = CallType::DrawShort;
    queue_try_add(&call_queue, &entry);
}