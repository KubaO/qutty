/*
 * Copyright (C) 2013 Rajendran Thirupugalsamy
 * See LICENSE for full copyright and license information.
 * See COPYING for distribution information.
 */

#ifndef GUIFINDTOOLBAR_H
#define GUIFINDTOOLBAR_H

#include <QLineEdit>
#include <QMetaType>
#include <QToolBar>
#include <QToolButton>

#include "TerminalSearch.hpp"

typedef struct terminal_tag Terminal;

class GuiFindToolBar : public QToolBar {
  Q_OBJECT
  Q_PROPERTY(TerminalSearch::Matches matches READ matches NOTIFY matchesChanged)
  Q_PROPERTY(int currentMatch READ currentMatch NOTIFY currentMatchChanged)

  QLineEdit m_pattern;
  QToolButton m_up;
  QToolButton m_down;
  QToolButton m_options;
  QToolButton m_close;

  TerminalSearch::Options m_searchOptions = {};
  TerminalSearch m_search;
  Terminal *m_prevTerminal = nullptr;
  QList<TerminalSearch::Match> m_matches;
  int m_curMatch = -1;

  bool searchIn(Terminal *term);
  void setCurMatch(int);
  void clearMatches();
  Terminal *terminal();

 public:
  explicit GuiFindToolBar(QWidget *parent, QMenu *findOptionsMenu);
  bool eventFilter(QObject *obj, QEvent *event) override;

  const TerminalSearch::Matches &matches() { return m_matches; }
  int currentMatch() const { return m_curMatch; }

 signals:
  void matchesChanged(const TerminalSearch::Matches &);
  void currentMatchChanged(int);

  Terminal *getTerminal();

 public slots:
  void reset();
  void findPrevious();
  void findNext();
  void close();

  void setRegex(bool);
  void setMultiLine(bool);
  void setCaseInsensitive(bool);
};

Q_DECLARE_METATYPE(TerminalSearch::Matches)

#endif  // GUIFINDTOOLBAR_H
