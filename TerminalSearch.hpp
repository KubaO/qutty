#ifndef TERMINALSEARCH_HPP
#define TERMINALSEARCH_HPP

#include <QFlags>
#include <QList>
#include <QString>

extern "C" {
#include "defs.h"
}

/*
 * A search object that can search for text in the terminal buffer,
 * and which contains the results of a search
 */
class TerminalSearch {
 public:
  struct RowCol {
    unsigned int row : 22;
    unsigned int col : 10;
    RowCol() = default;
    RowCol(unsigned int row, unsigned int col) : row(row), col(col) {}
  };

 private:
  struct Capture {
    QString text;
    QList<RowCol> locations;

    void clear() {
      text.clear();
      locations.clear();
    }
    int size() const {
      assert(text.size() == locations.size());
      return text.size();
    }
    bool isEmpty() const { return size() == 0; }
  };

 private:
  // Capture that treats the entire buffer as one continuous string, without line endings
  Capture m_capture;
  // Capture that treats the entire buffer as a concatenation of lines, with line endings
  Capture m_lineCapture;

 public:
  enum Option {
    None = 0,
    CaseInsensitive = 1,
    RegularExpression = 2,
    MultiLine = 4,
  };
  Q_DECLARE_FLAGS(Options, Option)

  /* Returns true when the option value has changed */
  static bool set(Options &opts, Option opt, bool to);

  class Match {
    QString m_textSource;

   public:
    RowCol start, end;
    QStringView text;

    explicit Match(const QString &str) : m_textSource(str), text(m_textSource) {}
    explicit Match(QStringView view) : text(view) {}
    Match(const Match &other) {
      if (other.m_textSource.isNull())
        text = other.text;
      else {
        m_textSource = other.m_textSource;
        text = m_textSource;
      }
      start = other.start, end = other.end;
    }
    Match(Match &&other) noexcept { operator=(std::move(other)); }
    Match &operator=(Match &&other) noexcept {
      if (other.m_textSource.isNull())
        text = other.text;
      else {
        m_textSource = std::move(other.m_textSource);
        text = m_textSource;
      }
      start = other.start, end = other.end;
      return *this;
    }
    Match &operator=(const Match &) = delete;
  };

  using Matches = QList<Match>;

  TerminalSearch() = default;
  explicit TerminalSearch(Terminal *term);
  void captureFrom(Terminal *term);
  void clearCapture();
  bool hasCapture() const { return !m_capture.isEmpty(); }

  /* Searches in full-text capture */
  Matches searchInCapture(const QString &pattern, Options options) const;

  /* Searches in individual lines */
  static Matches searchInLines(Terminal *term, const QString &pattern, Options options);

 private:
  static void fillMatch(Match &tsm, const QRegularExpressionMatch &rem, const Capture &capture);
  static Match matchViewFromREMatch(const QRegularExpressionMatch &match, const Capture &capture);
  static Match matchFromREMatch(const QRegularExpressionMatch &match, const Capture &capture);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(TerminalSearch::Options)

#endif  // TERMINALSEARCH_HPP
