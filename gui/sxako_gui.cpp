#include <ludisto/gui/svg_support.h>
#include <ludisto/general/utils.h>
#include <gtkmm.h>
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>

#include <fstream>


using namespace std;
using namespace sxako;
using namespace sxako::gui;
using namespace Gtk;

// notes about Mat & friends
//
// dispatchers are needed because all widget operations must be done in the
// widget thread; methods called from the player's thread (marked with "_d_t"
// for "other thread") set variables and then trigger the dispatcher

struct MatBoard {
  MatBoard() {
    dispatcher.connect(
      [this]() {
        { lock_guard<mutex> board_lock_guard(board_mutex);
          drawing_area.set_image(svg);
        }
      });
  }
  void set_d_t(string new_svg_board) { // called from the player's thread
    { lock_guard<mutex> board_lock_guard(board_mutex);
      svg=new_svg_board;
      dispatcher.emit();
    }
  }
  SVGDrawingArea drawing_area;
  string svg;
  Glib::Dispatcher dispatcher;
  mutex board_mutex;
};

struct MatMove {
  MatMove() {
    entry.signal_activate().connect(
      [this]() {
        string move_s=entry.get_text();
        entry.set_text("");
        send(move_s);
      });
  }
  string get_d_t() { // called from the player's thread
    string result;
    { unique_lock<mutex> move_unique_lock(move_mutex);
      move_cv.wait(move_unique_lock, [this]() { return not text.empty(); });
      result=text;
      text.clear();
    }
    move_cv.notify_one();
    return result;
  }
  void send(string m) {
    { lock_guard<mutex> move_lock_guard(move_mutex);
      text=m;
    }
    move_cv.notify_one();
  }
  string text;
  Entry entry;
  mutex move_mutex;
  condition_variable move_cv;
};

struct MatMessage {
  MatMessage() {
    window.add(view);
    window.set_policy(POLICY_AUTOMATIC, POLICY_AUTOMATIC);
    view.set_editable(false);
    view.set_wrap_mode(WRAP_WORD_CHAR);
    view.set_buffer(buffer);
    dispatcher.connect(
      [this]() {
        { lock_guard<mutex> message_lock_guard(message_mutex);
          buffer->set_text(text);
          view.scroll_to(buffer->get_insert());
        }
      });
  }
  void add(string new_text) { // called from the player's thread
    { lock_guard<mutex> message_lock_guard(message_mutex);
      text+=new_text;
      dispatcher.emit();
    }
  }
  ScrolledWindow window;
  TextView view;
  Glib::RefPtr<TextBuffer> buffer=TextBuffer::create();
  string text;
  Glib::Dispatcher dispatcher;
  mutex message_mutex;
};

class Mat
  : public Window {                       //  top----------------------------.
public:                                   //  | board------. | control-----. |
  Mat()                                   //  | |          | | | move      | |
    : top(ORIENTATION_HORIZONTAL),        //  | |          | | | undo      | |
      board_area(ORIENTATION_VERTICAL),   //  | |          | | | message-. | |
      control_area(ORIENTATION_VERTICAL), //  | |          | | | '-------' | |
      border(ORIENTATION_VERTICAL),       //  | |          | | | quit      | |
      undo("undo"), quit("quit") {        //  | '----------' | '-----------' |
    set_title("Ŝako's mat");              //  '------------------------------'
    add(top);
    { top.pack_start(board_area);
      board_area.set_border_width(border_width);
      board_area.pack_start(board.drawing_area);
    }
    top.pack_start(border, PACK_SHRINK);
    { top.pack_start(control_area);
      control_area.set_border_width(border_width);
      control_area.pack_start(move.entry, PACK_SHRINK);
      control_area.pack_start(undo, PACK_SHRINK);
      undo.signal_clicked().connect([this]() { move.send("/undo"); });
      control_area.pack_start(message.window);
      control_area.pack_start(quit, PACK_SHRINK);
      quit.signal_clicked().connect(
        [this]() { go_on_v=false; move.send("/quit"); hide(); });
    }
    show_all_children();
  }
  void set_board(string new_svg_board) { board.set_d_t(new_svg_board); }
  void add_message(string new_text) { message.add(new_text); }
  string get_move() { return move.get_d_t(); }
  bool go_on() const { return go_on_v; }
private:
  static int const border_width=10;
  Box top, board_area, control_area;
  Separator border;

  MatBoard board;
  MatMove move;
  MatMessage message;
  Button undo, quit;

  bool go_on_v=true;
};

int main(int argc, char *argv[])
{
  auto app=Application::create();

  Mat mat;

  input_f mat_input=[&mat]() { return mat.get_move(); };
  output_f board_output=[&mat](string b) { mat.set_board(b); };
  output_f message_output=[&mat](string m) { mat.add_message(m); };
  thread play_game_thread(
    [argc, argv, mat_input, board_output, message_output, &mat]() {
      play_game(argc, argv, mat_input, board_output, message_output,
                [&mat]() { return mat.go_on(); }, "svg");
    });

  int result=app->run(mat);

  play_game_thread.join();

  return result;
}
