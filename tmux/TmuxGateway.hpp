#ifndef TMUXGATEWAY_H
#define TMUXGATEWAY_H

#ifdef _WIN32
#include <msxml.h>  // otherwise marshal.h:get_data interferes with it
#endif

class GuiSplitter;
class GuiTerminalWindow;
class TmuxGateway;
class TmuxWindowPane;

extern "C" {
#include "misc.h"
// min/max interferes with std::min/max
#undef min
#undef max
#undef debug  // clashes with debug in qlogging.h
}
#include <map>
#include <queue>
#include <string>

#include "tmux/TmuxLayout.hpp"

#define is_hex_char(ch) \
  (((ch) >= '0' && (ch) <= '9') || ((ch) >= 'a' && (ch) <= 'f') || ((ch) >= 'A' && (ch) <= 'F'))

#define hex_to_char(ch)                                 \
  (((ch) >= '0' && (ch) <= '9')                         \
       ? (ch) - '0'                                     \
       : ((ch) >= 'a' && (ch) <= 'f') ? 10 + (ch) - 'a' \
                                      : ((ch) >= 'A' && (ch) <= 'F') ? 10 + (ch) - 'A' : 0)

#define TMUX_CB_INDEX_LIST                                                          \
  T(CB_NULL), T(CB_LIST_WINDOWS), T(CB_OPEN_LISTED_WINDOWS), T(CB_DUMP_TERM_STATE), \
      T(CB_DUMP_HISTORY), T(CB_DUMP_HISTORY_ALT), T(CB_INDEX_MAX)

enum tmux_cb_index_t {
#undef T
#define T(a) a
  TMUX_CB_INDEX_LIST
#undef T
};

extern const char *get_tmux_cb_index_str(tmux_cb_index_t index);

class TmuxCmdRespReceiver {
 public:
  virtual int performCallback(tmux_cb_index_t index, std::string &response) = 0;
  virtual ~TmuxCmdRespReceiver() {}
};

class TmuxCmdResp {
 public:
  TmuxCmdRespReceiver *receiver;
  tmux_cb_index_t callback;
  TmuxCmdResp(TmuxCmdRespReceiver *recv, tmux_cb_index_t cb) {
    receiver = recv;
    callback = cb;
  }
};

class TmuxGateway : public TmuxCmdRespReceiver {
  GuiTerminalWindow *termGatewayWnd;
  bufchain buffer;
  std::queue<TmuxCmdResp> _commandQueue;
  TmuxCmdResp _currentCommand;
  std::string _currentCommandResponse;
  long _sessionID;
  char *_sessionName;
  std::map<int, TmuxLayout> _mapLayout;
  std::map<int, TmuxWindowPane *> _mapPanes;

  void closeAllPanes();
  void closePane(int paneid);

  int cmd_hdlr_sessions_changed(const char *command, size_t len);
  int cmd_hdlr_session_changed(const char *command, size_t len);
  int cmd_hdlr_output(const char *command, size_t len);
  int cmd_hdlr_window_renamed(const char *command, size_t len);
  int cmd_hdlr_window_add(const char *command, size_t len);
  int cmd_hdlr_window_close(const char *command, size_t len);
  int cmd_hdlr_layout_change(const char *command, size_t len);

  int resp_hdlr_list_windows(std::string &response);
  int resp_hdlr_open_listed_windows(std::string &response);

 public:
  TmuxGateway(GuiTerminalWindow *termWindow);
  ~TmuxGateway() override;
  int performCallback(tmux_cb_index_t index, std::string &response) override;
  size_t fromBackend(int is_stderr, const char *data, size_t len);
  int parseCommand(const char *command, size_t len);
  int openWindowsInitial();
  int createNewWindow(int id, const char *name, int width, int height, const std::string &layout);
  int createNewWindowPane(int id, const char *name, const TmuxLayout &layout,
                          GuiSplitter *splitter);
  int sendCommand(TmuxCmdRespReceiver *recv, tmux_cb_index_t cb, const wchar_t cmd_str[],
                  size_t cmd_str_len);
  int sendCommand(TmuxCmdRespReceiver *recv, tmux_cb_index_t cb, const wchar_t cmd_str[]);
  int sendCommand(TmuxCmdResp cmd_list[], std::wstring cmd_str[], int len = 1);

  void sendCommandNewWindowInSession();

  void initiateDetach();
  void detach();
};

#endif  // TMUXGATEWAY_H
