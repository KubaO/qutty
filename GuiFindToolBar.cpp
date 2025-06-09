/*
 * Copyright (C) 2013 Rajendran Thirupugalsamy
 * See LICENSE for full copyright and license information.
 * See COPYING for distribution information.
 */

#include "GuiFindToolBar.hpp"

#include <QKeyEvent>
#include <QScrollBar>

GuiFindToolBar::GuiFindToolBar(QWidget *parent, QMenu *findOptionsMenu) : QToolBar(parent) {
  qRegisterMetaType<TerminalSearch::Matches>();

  addWidget(&m_pattern);
  connect(&m_pattern, &QLineEdit::textChanged, this, &GuiFindToolBar::reset);
  m_pattern.installEventFilter(this);

  m_up.setText("Up");
  connect(&m_up, &QToolButton::clicked, this, &GuiFindToolBar::findPrevious);
  addWidget(&m_up);

  m_down.setText("Down");
  connect(&m_down, &QToolButton::clicked, this, &GuiFindToolBar::findNext);
  addWidget(&m_down);

  m_options.setIcon(QIcon(":/images/cog_alt_16x16.png"));
  m_options.setMenu(findOptionsMenu);
  m_options.setPopupMode(QToolButton::InstantPopup);
  addWidget(&m_options);

  m_close.setIcon(QIcon(":/images/x_14x14.png"));
  connect(&m_close, &QToolButton::clicked, this, &GuiFindToolBar::close);
  addWidget(&m_close);

  setIconSize(QSize(16, 16));
  setMovable(false);
  setAutoFillBackground(true);
  adjustSize();

  m_pattern.setFocus();
}

constexpr auto set = TerminalSearch::set;
using Option = TerminalSearch::Option;

void GuiFindToolBar::setRegex(bool regex) {
  if (set(m_searchOptions, Option::RegularExpression, regex)) reset();
}

void GuiFindToolBar::setMultiLine(bool ml) {
  if (set(m_searchOptions, Option::MultiLine, ml)) reset();
}
void GuiFindToolBar::setCaseInsensitive(bool ci) {
  if (set(m_searchOptions, Option::CaseInsensitive, ci)) reset();
}

bool GuiFindToolBar::eventFilter(QObject *obj, QEvent *event) {
  if (obj == &m_pattern) {
    if (event->type() == QEvent::KeyPress) {
      QKeyEvent *ke = (QKeyEvent *)event;
      if (ke->key() == Qt::Key_Escape) {
        close();
        return true;
      } else if (ke->key() == Qt::Key_Return) {
        if (ke->modifiers() & Qt::ShiftModifier)
          findPrevious();
        else
          findNext();
        return true;
      }
    }
  }
  return false;
}

void GuiFindToolBar::setCurMatch(int curMatch) {
  if (m_curMatch != curMatch) {
    m_curMatch = curMatch;
    emit currentMatchChanged(m_curMatch);
  }
}

void GuiFindToolBar::clearMatches() {
  if (!m_matches.isEmpty()) {
    m_matches.clear();
    emit matchesChanged(m_matches);
  }
}

void GuiFindToolBar::reset() {
  m_search.clearCapture();
  clearMatches();
  setCurMatch(-1);
}

// Returns true if the matches have changed
bool GuiFindToolBar::searchIn(Terminal *term) {
  if (m_matches.isEmpty()) {
    if (m_searchOptions & Option::MultiLine) {
      if (!m_search.hasCapture()) m_search.captureFrom(term);
      m_matches = m_search.searchInCapture(m_pattern.text(), m_searchOptions);
    } else {
      m_matches = TerminalSearch::searchInLines(term, m_pattern.text(), m_searchOptions);
    }
    emit matchesChanged(m_matches);
    return true;
  }
  return false;
}

Terminal *GuiFindToolBar::terminal() {
  Terminal *term = emit getTerminal();
  if (term != m_prevTerminal) {
    m_prevTerminal = term;
    reset();
  }
  return term;
}

void GuiFindToolBar::findPrevious() {
  Terminal *term = terminal();
  if (!term) return;
  int curMatch = m_curMatch;

  if (searchIn(term))
    curMatch = m_matches.size() - 1;
  else if (m_curMatch > 0)
    curMatch--;
  else
    return;

  if (m_matches.empty()) curMatch = -1;
  setCurMatch(curMatch);
}

void GuiFindToolBar::findNext() {
  Terminal *term = terminal();
  if (!term) return;
  int curMatch = m_curMatch;

  if (searchIn(term))
    curMatch = 0;
  else if (m_curMatch < (m_matches.size() - 1))
    curMatch++;
  else
    return;

  if (m_matches.empty()) curMatch = -1;
  setCurMatch(curMatch);
}

void GuiFindToolBar::close() { this->deleteLater(); }
