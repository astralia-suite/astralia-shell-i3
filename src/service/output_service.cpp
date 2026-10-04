#include <utility>
#include <xcb/randr.h>

#include "service/output_service.h"

namespace astralia {

namespace {

std::vector<Output> current_outputs(const XConnection &x) {
    std::vector<Output> outputs = x.outputs();
    if (outputs.empty()) {
        xcb_screen_t *screen = x.screen();
        outputs.push_back({"", {0, 0, screen->width_in_pixels, screen->height_in_pixels}});
    }
    return outputs;
}

} // namespace

OutputService::OutputService(XConnection &x, EventLoop &loop)
    : x_(x), outputs_(current_outputs(x)) {
    const xcb_query_extension_reply_t *randr = xcb_get_extension_data(x_.conn(), &xcb_randr_id);
    if (randr == nullptr || !randr->present) {
        return;
    }
    xcb_randr_select_input(x_.conn(), x_.root(), XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE);
    xcb_flush(x_.conn());
    loop.on_event(randr->first_event + XCB_RANDR_SCREEN_CHANGE_NOTIFY,
                  [this](const xcb_generic_event_t &) { refresh(); });
}

const Output &OutputService::at_pointer() const {
    auto [x, y] = x_.pointer_position();
    for (const Output &output : outputs_) {
        const OutputGeometry &g = output.geometry;
        if (x >= g.x && x < g.x + g.width && y >= g.y && y < g.y + g.height) {
            return output;
        }
    }
    return outputs_.front();
}

void OutputService::refresh() {
    std::vector<Output> next = current_outputs(x_);
    if (next == outputs_) {
        return;
    }
    outputs_ = std::move(next);
    changed.emit();
}

} // namespace astralia
