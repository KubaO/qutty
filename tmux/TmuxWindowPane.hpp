#ifndef TMUXWINDOWPANE_H
#define TMUXWINDOWPANE_H

#include "tmux/TmuxGateway.hpp"
#include "tmux/tmux.h"

// -- begin section copied from tmux.h --
/* Grid attributes. */
#define GRID_ATTR_BRIGHT 0x1
#define GRID_ATTR_DIM 0x2
#define GRID_ATTR_UNDERSCORE 0x4
#define GRID_ATTR_BLINK 0x8
#define GRID_ATTR_REVERSE 0x10
#define GRID_ATTR_HIDDEN 0x20
#define GRID_ATTR_ITALICS 0x40
#define GRID_ATTR_CHARSET 0x80 /* alternative character set */

class TmuxWindowPane : public TmuxCmdRespReceiver {
  TmuxGateway *_gateway;
  GuiTerminalWindow *_termWnd;

 public:
  int id;
  std::string name;
  int width;
  int height;
  bool ready;

  TmuxWindowPane(TmuxGateway *gateway, GuiTerminalWindow *termwnd);
  ~TmuxWindowPane() override;

  TmuxGateway *gateway() { return _gateway; }
  GuiTerminalWindow *termWnd() { return _termWnd; }
  int performCallback(tmux_cb_index_t index, std::string &response) override;

  int resp_hdlr_dump_term_state(std::string &response);
  int resp_hdlr_dump_history(std::string &response, bool is_alt = false);
};

#endif  // TMUXWINDOWPANE_H
