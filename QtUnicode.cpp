/*
 * Copyright (C) 2012 Rajendran Thirupugalsamy
 * See LICENSE for full copyright and license information.
 * See COPYING for distribution information.
 */

extern "C" {
#include "putty.h"
}
#undef debug  // clashes with debug in qlogging.h

#include <QDebug>
#include <QTextCodec>

#include "QtConfig.hpp"

#define CS_QTEXTCODEC 11111111

struct QtCodec {
  int intCodepage; /* first one is CS_QTEXTCODEC, next is CS_QTEXTCODEC+1, etc. */
  std::string strCodepage;
  QTextCodec *codec;

  QtCodec(int intCodepage, std::string_view strCodepage, QTextCodec *codec)
      : intCodepage(intCodepage), strCodepage(strCodepage), codec(codec) {}
};

std::vector<QtCodec> qtCodecs;

static QTextCodec *getTextCodec(const std::string &lineCodepage, int *codepageOut) {
  if (codepageOut) *codepageOut = -1;

  for (auto const &qtCodec : qtCodecs) {
    if (qtCodec.strCodepage == lineCodepage) {
      if (codepageOut) *codepageOut = qtCodec.intCodepage;
      return qtCodec.codec;
    }
  }

  QTextCodec *codec = nullptr;
  if (!lineCodepage.empty()) {
    qDebug() << __FUNCTION__ << " using line_codepage " << lineCodepage;
    codec = QTextCodec::codecForName(lineCodepage.c_str());
    if (!codec) {
      std::string codepage = lineCodepage;
      if (strtok(codepage.data(), ":(")) codec = QTextCodec::codecForName(codepage.c_str());
    }
  }
  if (!codec) codec = QTextCodec::codecForLocale();
  if (!codec) codec = QTextCodec::codecForUtfText(APPNAME);
  qDebug() << __FUNCTION__ << " using codec " << (codec ? codec->name().constData() : "NULL");

  int intLineCodepage = CS_QTEXTCODEC + qtCodecs.size();
  qtCodecs.emplace_back(intLineCodepage, lineCodepage, codec);
  if (lineCodepage == "UTF-8") {
    intLineCodepage = CP_UTF8;
    qtCodecs.emplace_back(intLineCodepage, lineCodepage, codec);
  }
  if (codepageOut) *codepageOut = intLineCodepage;

  return codec;
}

QTextCodec *getTextCodec(int codepage) {
#ifdef _WIN32
  // Translate Windows-specific default code pages
  switch (codepage) {
    case CP_ACP:
      codepage = GetACP();
      break;
    case CP_OEMCP:
      codepage = GetOEMCP();
      break;
    default:
      break;
  }
#endif
  for (auto const &qtCodec : qtCodecs)
    if (qtCodec.intCodepage == codepage) return qtCodec.codec;
  return nullptr;
}

/* TODO
 * This one is meant to use platform-independent QTextCodec.
 * For now we can use the platform-specific implementation.
 * */
void init_ucs(Conf *cfg, struct unicode_data *ucsdata) {
  int i;

  /*
   * In the platform-independent parts of the code, font_codepage
   * is used only for system DBCS support - which we don't
   * support at all. We set it to line_codepage below, so that
   * multibyte<->wide conversions work with either line_codepage
   * or font_codepage.
   */
  ucsdata->font_codepage = -1;
  const char *line_codepage = "";
  if (confKeyExists(cfg, CONF_line_codepage)) line_codepage = conf_get_str(cfg, CONF_line_codepage);

  QTextCodec *encoder = getTextCodec(line_codepage, &ucsdata->line_codepage);
  if (!encoder) modalfatalbox("Unable to get a QTextCodec for codepage %s", line_codepage);

  ucsdata->font_codepage = ucsdata->line_codepage;

  /*
   * Set up unitab_line, by translating each individual character
   * in the line codepage into Unicode.
   */
  for (i = 0; i < 256; i++) {
    char c[1], *p;
    wchar_t wc[1];
    int len;
    c[0] = (char)i;
    p = c;
    len = 1;
    ucsdata->unitab_line[i] = i;
    if (mb_to_wc(ucsdata->line_codepage, 0, c, 1, wc, 1) == 1)
      ucsdata->unitab_line[i] = wc[0];
    else
      ucsdata->unitab_line[i] = 0xFFFD;
  }

  /*
   * Set up unitab_xterm. This is the same as unitab_line except
   * in the line-drawing regions, where it follows the Unicode
   * encoding.
   *
   * (Note that the strange X encoding of line-drawing characters
   * in the bottom 32 glyphs of ISO8859-1 fonts is taken care of
   * by the font encoding, which will spot such a font and act as
   * if it were in a variant encoding of ISO8859-1.)
   */
  for (i = 0; i < 256; i++) {
    static const wchar_t unitab_xterm_std[32] = {
        0x2666, 0x2592, 0x2409, 0x240c, 0x240d, 0x240a, 0x00b0, 0x00b1, 0x2424, 0x240b, 0x2518,
        0x2510, 0x250c, 0x2514, 0x253c, 0x23ba, 0x23bb, 0x2500, 0x23bc, 0x23bd, 0x251c, 0x2524,
        0x2534, 0x252c, 0x2502, 0x2264, 0x2265, 0x03c0, 0x2260, 0x00a3, 0x00b7, 0x0020};
    static const wchar_t unitab_xterm_poorman[33] = L"*#****o~**+++++-----++++|****L. ";

    const wchar_t *ptr;

    if (conf_get_int(cfg, CONF_vtmode) == VT_POORMAN)
      ptr = unitab_xterm_poorman;
    else
      ptr = unitab_xterm_std;

    if (i >= 0x5F && i < 0x7F)
      ucsdata->unitab_xterm[i] = ptr[i & 0x1F];
    else
      ucsdata->unitab_xterm[i] = ucsdata->unitab_line[i];
  }

  /*
   * Set up unitab_scoacs. The SCO Alternate Character Set is
   * simply CP437.
   */
  for (i = 0; i < 256; i++) {
    char c[1], *p;
    int len;
    c[0] = (char)i;
    p = c;
    len = 1;
    ucsdata->unitab_scoacs[i] = i;
    /*if (1 == charset_to_unicode(&p, &len, wc, 1, CS_CP437, NULL, L"", 0))
        ucsdata->unitab_scoacs[i] = wc[0];
    else
        ucsdata->unitab_scoacs[i] = 0xFFFD;*/
  }

  /*
   * Find the control characters in the line codepage. For
   * direct-to-font mode using the D800 hack, we assume 00-1F and
   * 7F are controls, but allow 80-9F through. (It's as good a
   * guess as anything; and my bet is that half the weird fonts
   * used in this way will be IBM or MS code pages anyway.)
   */
  for (i = 0; i < 256; i++) {
    int lineval = ucsdata->unitab_line[i];
    if (lineval < ' ' || (lineval >= 0x7F && lineval < 0xA0) ||
        (lineval >= 0xD800 && lineval < 0xD820) || (lineval == 0xD87F))
      ucsdata->unitab_ctrl[i] = (unsigned char)i;
    else
      ucsdata->unitab_ctrl[i] = 0xFF;
  }

  ucsdata->dbcs_screenfont = 0;
}
