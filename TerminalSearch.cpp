#include "TerminalSearch.hpp"

#include <QRegularExpression>

#include "QtCommon.hpp"

extern "C" {
#include "putty.h"
#include "terminal.h"
}

TerminalSearch::TerminalSearch(Terminal *term) { captureFrom(term); }

static int sblines(Terminal *term) { return count234(term->scrollback); }

void TerminalSearch::captureFrom(Terminal *term) {
  int startRow = -sblines(term);
  int endRow = term->rows - 1;

  clearCapture();

  QList<bool> advances;

  for (int row = startRow; row < endRow; row++) {
    termline *line = term_get_line(term, row);
    const int added = decodeFromTerminal(term, line, m_capture.text, &advances);
    term_release_line(line);
    assert(advances.size() == added);

    QStringView addedLine = QStringView(m_capture.text).sliced(m_capture.size() - added);
    m_lineCapture.text.append(addedLine);

    int col = 0;
    for (const bool &advance : std::as_const(advances)) {
      m_capture.locations.emplace_back(row, col);
      m_lineCapture.locations.emplace_back(row, col);
      if (advance) col++;
    }

    m_lineCapture.text.append('\n');
    m_lineCapture.locations.emplace_back(m_lineCapture.locations.back());
  }

  assert(m_capture.size() == m_capture.locations.size());
  assert(m_lineCapture.size() == m_lineCapture.locations.size());
}

void TerminalSearch::clearCapture() {
  m_capture.clear();
  m_lineCapture.clear();
}

static QRegularExpression makeRegex(const QString &pattern, TerminalSearch::Options options) {
  QRegularExpression re;
  if (options & TerminalSearch::Option::RegularExpression)
    re.setPattern(pattern);
  else
    re.setPattern(QRegularExpression::escape(pattern));
  if (options & TerminalSearch::Option::CaseInsensitive)
    re.setPatternOptions(QRegularExpression::PatternOption::CaseInsensitiveOption);
  if (options & TerminalSearch::Option::MultiLine)
    re.setPatternOptions(QRegularExpression::PatternOption::MultilineOption);
  return re;
}

void TerminalSearch::fillMatch(Match &tsm, const QRegularExpressionMatch &rem,
                               const Capture &capture) {
  tsm.start = capture.locations[rem.capturedStart()];
  tsm.end = capture.locations[rem.capturedEnd()];
}

TerminalSearch::Match TerminalSearch::matchViewFromREMatch(const QRegularExpressionMatch &match,
                                                           const Capture &capture) {
  Match result(match.capturedView());
  fillMatch(result, match, capture);
  return result;
}

TerminalSearch::Match TerminalSearch::matchFromREMatch(const QRegularExpressionMatch &match,
                                                       const Capture &capture) {
  Match result(match.captured());
  fillMatch(result, match, capture);
  return result;
}

TerminalSearch::Matches TerminalSearch::searchInCapture(const QString &pattern,
                                                        Options options) const {
  const Capture &capture =
      (options & TerminalSearch::Option::MultiLine) ? m_lineCapture : m_capture;
  Matches matches;
  QRegularExpression re = makeRegex(pattern, options);

  auto it = re.globalMatch(capture.text);
  while (it.hasNext()) {
    auto match = it.next();
    if (!match.hasMatch()) continue;
    // Make view matches, since we own a full capture
    matches.push_back(matchViewFromREMatch(match, capture));
  }
  return matches;
}

TerminalSearch::Matches TerminalSearch::searchInLines(Terminal *term, const QString &pattern,
                                                      Options options) {
  assert(!(options & TerminalSearch::Option::MultiLine));

  int startRow = -sblines(term);
  int endRow = term->rows - 1;

  Matches matches;
  QRegularExpression re = makeRegex(pattern, options);
  Capture capture;
  QList<bool> advances;

  for (int row = startRow; row < endRow; row++) {
    capture.clear();

    termline *line = term_get_line(term, row);
    const int added = decodeFromTerminal(term, line, capture.text, &advances);
    term_release_line(line);
    assert(advances.size() == added);

    auto it = re.globalMatch(capture.text);
    if (!it.hasNext()) continue;

    int col = 0;
    for (const bool &advance : std::as_const(advances)) {
      capture.locations.emplace_back(row, col);
      if (advance) col++;
    }

    while (it.hasNext()) {
      auto match = it.next();
      if (!match.hasMatch()) continue;
      // Make string-carrying matches, since the capture is ephemeral
      matches.push_back(matchFromREMatch(match, capture));
    }
  }
  return matches;
}

bool TerminalSearch::set(Options &opts, Option opt, bool to) {
  if (opts.testFlag(opt) != to) {
    opts.setFlag(opt, to);
    return true;
  }
  return false;
}
