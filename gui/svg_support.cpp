#include "svg_support.h"
#include <librsvg/rsvg.h>

using namespace std;

namespace sxako {
namespace gui {

  void SVGDrawingArea::set_image(string new_svg_text) {
    svg_text=new_svg_text;
    // invalidate whole area to force redraw:
    if (auto window=get_window())
      window->invalidate_rect(whole_board(), false);
  }

  bool SVGDrawingArea::on_draw(Cairo::RefPtr<Cairo::Context> const &cr) {
    DrawingArea::on_draw(cr);

    auto handle= // FIXME: don't we have to free() this eventually?
      rsvg_handle_new_from_data(
        reinterpret_cast<guint8 const *>(svg_text.c_str()),
        svg_text.length(),
        nullptr);
    RsvgDimensionData svg_dimensions;
    rsvg_handle_get_dimensions(handle, &svg_dimensions);

    if (not size_initialized) { // the first time, set natural size
      set_size_request(svg_dimensions.width, svg_dimensions.height);
      size_initialized=true;
    }

    auto area_dimensions=whole_board();
    auto scale=min(double(area_dimensions.get_width())/svg_dimensions.width,
                   double(area_dimensions.get_height())/svg_dimensions.height);
    cr->scale(scale, scale);
    rsvg_handle_render_cairo(handle, cr->cobj());
    return true;
  }

  Gdk::Rectangle SVGDrawingArea::whole_board() const {
    auto alloc=get_allocation();
    return {0, 0, alloc.get_width(), alloc.get_height()};
  }

}
}
