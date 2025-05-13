#ifndef TMUXLAYOUT_H
#define TMUXLAYOUT_H

#include <sstream>
#include <string>
#include <vector>

class TmuxLayout {
 public:
  unsigned int width = 0;
  unsigned int height = 0;
  unsigned int x = 0;
  unsigned int y = 0;
  unsigned int paneid = -1;
  enum TmuxLayoutType {
    TMUX_LAYOUT_TYPE_NONE = -1,
    TMUX_LAYOUT_TYPE_LEAF,
    TMUX_LAYOUT_TYPE_HORIZONTAL,
    TMUX_LAYOUT_TYPE_VERTICAL
  };
  TmuxLayoutType layoutType = TMUX_LAYOUT_TYPE_NONE;
  std::vector<TmuxLayout> child;
  TmuxLayout *parent = nullptr;

 public:
  bool initLayout(const std::string &layout) {
    std::istringstream iresp(layout, std::istringstream::in);
    return parseLayout(iresp);
  }

  std::string dumpLayout();

 private:
  bool parseLayout(std::istringstream &iresp);
  bool parseLayoutChildren(std::istringstream &iresp);
};

#endif  // TMUXLAYOUT_H
