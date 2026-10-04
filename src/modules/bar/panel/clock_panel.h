#pragma once

#include <array>
#include <vector>
#include <xcb/xcb.h>

#include "core/event_loop.h"
#include "core/x_connection.h"

#include "render/panel_chrome.h"
#include "render/panel_window.h"

namespace astralia {

struct CalendarDay {
    int year;
    int month;
    int day;
    bool in_month;
};

struct CalendarMonth {
    int year;
    int month;
};

std::array<CalendarDay, 42> clock_panel_cells(int year, int month);
CalendarMonth clock_panel_month_shifted(int year, int month, int delta);
bool clock_panel_same_day(const CalendarDay &cell, int year, int month, int day);
int clock_panel_iso_week(int year, int month, int day);

class ClockPanel {
  public:
    ClockPanel(XConnection &x, EventLoop &loop);

    void toggle();
    PanelWindow &window() { return window_; }

  private:
    enum Action { previous = 1,
                  today,
                  next };

    void handle(const xcb_generic_event_t &event);
    void paint();

    PanelWindow window_;
    std::vector<PanelHit> hits_;
    int month_offset_ = 0;
};

} // namespace astralia
