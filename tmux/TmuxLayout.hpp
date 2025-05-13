#ifndef TMUXLAYOUT_H
#define TMUXLAYOUT_H

#include <sstream>
#include <string>
#include <vector>

class TmuxLayout {
 public:
  unsigned int width;
  unsigned int height;
  unsigned int x;
  unsigned int y;
  unsigned int paneid;
  enum TmuxLayoutType {
    TMUX_LAYOUT_TYPE_NONE = -1,
    TMUX_LAYOUT_TYPE_LEAF,
    TMUX_LAYOUT_TYPE_HORIZONTAL,
    TMUX_LAYOUT_TYPE_VERTICAL
  };
  TmuxLayoutType layoutType;
  std::vector<TmuxLayout> child;
  TmuxLayout *parent;

 public:
  TmuxLayout() {
    paneid = -1;
    width = height = 0;
    x = y = 0;
    parent = NULL;
    layoutType = TMUX_LAYOUT_TYPE_NONE;
  }

  bool initLayout(std::string layout) {
    std::istringstream iresp(layout, std::istringstream::in);
    return parseLayout(iresp);
  }

  std::string dumpLayout();

 private:
  bool parseLayout(std::istringstream &iresp);
  bool parseLayoutChildren(std::istringstream &iresp);
};

#endif  // TMUXLAYOUT_H
