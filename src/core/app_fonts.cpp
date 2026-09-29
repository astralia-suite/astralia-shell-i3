#include <array>
#include <fontconfig/fontconfig.h>
#include <string>

#include "core/app_fonts.h"
#include "core/log.h"

namespace astralia {

namespace {

bool add_font(const char *name) {
    constexpr std::array<const char *, 2> dirs{ASTRALIA_FONT_DIR, ASTRALIA_SOURCE_FONT_DIR};
    for (const char *dir : dirs) {
        std::string path = std::string(dir) + "/" + name;
        if (FcConfigAppFontAddFile(nullptr, reinterpret_cast<const FcChar8 *>(path.c_str()))) {
            return true;
        }
    }
    log::error("cannot load the font {}/{}", dirs[0], name);
    return false;
}

} // namespace

bool register_app_fonts() {
    static const bool registered = [] {
        bool icons = add_font("tabler-icons.ttf");
        bool text = add_font("ComicShannsMono-Regular.otf");
        bool glyphs = add_font("YujiMai.ttf");
        return icons && text && glyphs;
    }();
    return registered;
}

} // namespace astralia
