#ifndef SXAKO_GUI_SVG_SUPPORT_HEADER_
#define SXAKO_GUI_SVG_SUPPORT_HEADER_

#include <gtkmm/drawingarea.h>

namespace sxako {
namespace gui {

  // "SVGDrawingArea" is a drawing area that displays an SVG image, described
  // by its text contents, which is set by "set_image()"; the first time
  // "set_image()" is called, the size of the SVG image sets the size of the
  // drawing area; after that, the image is always resized to fit the size of
  // the drawing area
  class SVGDrawingArea
    : public Gtk::DrawingArea {
  public:
    void set_image(std::string new_svg_text);
  protected:
    bool on_draw(Cairo::RefPtr<Cairo::Context> const &cr) override;
  private:
    bool size_initialized=false;
    std::string svg_text;
    Gdk::Rectangle whole_board() const;
  };

}
}

#endif
