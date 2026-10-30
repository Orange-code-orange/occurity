/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/***************************************************************************
 *            screensaver.cpp
 *
 *  Animated OPTILAZ screensaver overlay
 ****************************************************************************/

/*
 *  This file is part of Occurity.
 *
 *  Occurity is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Occurity is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Occurity; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
 */
#include "screensaver.h"

#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QFont>
#include <QFontMetricsF>

#include <cmath>

// ---------------------------------------------------------------------------
// Virtual canvas and OPTILAZ brand constants (mirrors the reference JSX).
// ---------------------------------------------------------------------------
namespace {

const qreal CW = 1920.0;   // canvas width
const qreal CH = 1080.0;   // canvas height
const qreal TOTAL = 29.0;  // loop length in seconds

// Theme "Oscuro".
const QColor BG   (0x0B, 0x17, 0x33);  // #0B1733
const QColor BG2  (0x16, 0x30, 0x6A);  // #16306A
const QColor INK  (0xFF, 0xFF, 0xFF);  // #FFFFFF
const QColor MUTED(0xAE, 0xBB, 0xD6);  // #AEBBD6
const QColor BLOB (0x2B, 0x4E, 0x99);  // #2B4E99
const QColor YELLOW(0xF2, 0xD9, 0x5F); // #F2D95F

// Scene cue times (seconds from loop start).
const qreal A  = 5.0;   // Agudeza
const qreal M  = 14.0;  // Mirada
const qreal KC = 20.0;  // Marca
const qreal C  = 26.0;  // Cierre

// Logo geometry, derived from the 2992px source artwork (K scales it to the
// 760px on-canvas logo width).
const qreal K = 760.0 / 2992.0;
const qreal LOGO_LEFT = (CW - 760.0) / 2.0;   // 580
const qreal LOGO_TOP = 196.0;

const qreal DOT_X = LOGO_LEFT + 1474.0 * K;   // ~954.4
const qreal DOT_Y = LOGO_TOP  + 734.0 * K;    // ~382.4
const qreal DOT_R = 163.0 * K;                // ~41.4

const qreal WORD_X = LOGO_LEFT + (756.0 - 778.0) * K;
const qreal WORD_Y = LOGO_TOP  + (2854.0 - 1076.0) * K;
const qreal WORD_W = 3038.0 * K;
const qreal WORD_H = 564.0 * K;
const qreal TAG_Y  = WORD_Y + WORD_H + 64.0;

const qreal CURSOR_X = 560.0;

struct Row { const char *l; qreal s; qreal y; const char *lab; };
const Row ROWS[] = {
  { "E",     150.0, 320.0, "20/200" },
  { "FP",    108.0, 500.0, "20/100" },
  { "TOZ",    78.0, 630.0, "20/70"  },
  { "LPED",   58.0, 730.0, "20/50"  },
  { "PECFD",  44.0, 815.0, "20/40"  },
};
const int ROW_COUNT = 5;

// The tagline shown in the "Marca" scene (window.TWEAK_DEFAULTS in the source).
const char *TAGLINE = "Tu visión, nuestra prioridad";

// ------------------------- easing + tween helpers --------------------------

inline qreal clamp01(qreal v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }

inline qreal easeOutCubic(qreal p) { return 1.0 - std::pow(1.0 - p, 3.0); }

inline qreal easeInOutCubic(qreal p)
{
  return p < 0.5 ? 4.0 * p * p * p
                 : 1.0 - std::pow(-2.0 * p + 2.0, 3.0) / 2.0;
}

inline qreal easeOutBack(qreal p)
{
  const qreal c1 = 1.70158;
  const qreal c3 = c1 + 1.0;
  const qreal x = p - 1.0;
  return 1.0 + c3 * x * x * x + c1 * x * x;
}

enum Ease { Enter, Draw, Pop };

// Replicates the reference `animate({from,to,start,end,ease})(T)`: linearly
// normalises T to [0,1] over [start,end] and applies the easing.
qreal tween(qreal from, qreal to, qreal start, qreal end, qreal T, Ease e)
{
  if(end <= start) {
    return T < start ? from : to;
  }
  qreal p = clamp01((T - start) / (end - start));
  switch(e) {
  case Enter: p = easeOutCubic(p); break;
  case Draw:  p = easeInOutCubic(p); break;
  case Pop:   p = easeOutBack(p); break;
  }
  return from + (to - from) * p;
}

QColor withAlpha(QColor c, qreal a)
{
  c.setAlphaF(static_cast<float>(clamp01(a)));
  return c;
}

QFont brandFont(qreal pixelSize, int weight)
{
  QFont f("Manrope");
  f.setPixelSize(qMax(1, static_cast<int>(std::lround(pixelSize))));
  f.setWeight(static_cast<QFont::Weight>(weight));
  f.setStyleStrategy(QFont::PreferAntialias);
  return f;
}

// Draws a filled disc that, when 'blur' > 0, fades out over a soft radial
// gradient -- a cheap stand-in for a gaussian "defocus" that keeps 60 fps on a
// Raspberry Pi.
void softDisc(QPainter &p, qreal x, qreal y, qreal r, qreal blur,
              const QColor &color, qreal opacity)
{
  if(r <= 0.0 || opacity <= 0.0) {
    return;
  }
  p.setPen(Qt::NoPen);
  if(blur <= 0.5) {
    p.setBrush(withAlpha(color, opacity));
    p.drawEllipse(QPointF(x, y), r, r);
    return;
  }
  const qreal outer = r + blur;
  QRadialGradient g(QPointF(x, y), outer);
  g.setColorAt(0.0, withAlpha(color, opacity));
  g.setColorAt(clamp01(r / outer), withAlpha(color, opacity * 0.65));
  g.setColorAt(1.0, withAlpha(color, 0.0));
  p.setBrush(g);
  p.drawEllipse(QPointF(x, y), outer, outer);
}

// A soft, blurred background "blob" rendered as a radial gradient.
void blob(QPainter &p, qreal cx, qreal cy, qreal radius,
          const QColor &color, qreal opacity)
{
  QRadialGradient g(QPointF(cx, cy), radius);
  g.setColorAt(0.0, withAlpha(color, opacity));
  g.setColorAt(0.5, withAlpha(color, opacity * 0.75));
  g.setColorAt(1.0, withAlpha(color, 0.0));
  p.setPen(Qt::NoPen);
  p.setBrush(g);
  p.drawEllipse(QPointF(cx, cy), radius, radius);
}

// --------------------------- scene components ------------------------------

void drawBackground(QPainter &p, qreal T)
{
  const qreal ph = (T / TOTAL) * M_PI * 2.0;

  QRadialGradient grad(QPointF(CW * 0.5, CH * 0.45), CW * 0.72);
  grad.setColorAt(0.0, BG2);
  grad.setColorAt(0.7, BG);
  grad.setColorAt(1.0, BG);
  p.setPen(Qt::NoPen);
  p.setBrush(grad);
  p.drawRect(QRectF(0, 0, CW, CH));

  // Two slowly orbiting blurred blobs.
  blob(p, 200.0 + std::cos(ph) * 220.0 + 500.0,
          -300.0 + std::sin(ph) * 140.0 + 500.0,
          500.0 + 140.0, BLOB, 0.35);
  blob(p, 1100.0 + std::sin(ph) * 200.0 + 400.0,
          500.0 + std::cos(ph) * 120.0 + 400.0,
          400.0 + 160.0, YELLOW, 0.07);
}

void drawRing(QPainter &p, qreal T, qreal at, qreal x, qreal y,
              qreal maxR, qreal dur, qreal strength)
{
  if(T < at || T > at + dur) {
    return;
  }
  const qreal r = tween(DOT_R, maxR, at, at + dur, T, Enter);
  const qreal o = tween(strength, 0.0, at, at + dur, T, Draw);
  p.setBrush(Qt::NoBrush);
  p.setPen(QPen(withAlpha(YELLOW, o), 2.0));
  p.drawEllipse(QPointF(x, y), r, r);
}

qreal chartScale(qreal T)
{
  return T < A + 7.4 ? tween(1.0, 1.06, A, A + 7.4, T, Draw)
                     : tween(1.06, 1.22, A + 7.4, A + 9.0, T, Draw);
}

qreal cursorRawY(qreal T)
{
  qreal y = tween(540.0, ROWS[0].y, A - 0.3, A + 0.9, T, Draw);
  for(int i = 1; i < ROW_COUNT; ++i) {
    const qreal ti = A + 0.4 + i * 1.2;
    if(T >= ti - 0.35) {
      y = tween(ROWS[i - 1].y, ROWS[i].y, ti - 0.35, ti + 0.35, T, Draw);
    }
  }
  return y;
}

void drawChart(QPainter &p, qreal T)
{
  if(T < A - 0.5 || T > M + 0.5) {
    return;
  }
  const qreal s = chartScale(T);
  const qreal exitOp = tween(1.0, 0.0, A + 7.6, A + 9.0, T, Draw);
  const qreal bandY = cursorRawY(T);
  const qreal bandO = tween(0.0, 1.0, A + 0.3, A + 1.0, T, Enter);

  p.save();
  p.setOpacity(exitOp);
  p.translate(960.0, 540.0);
  p.scale(s, s);
  p.translate(-960.0, -540.0);

  // Highlight band following the cursor.
  p.setPen(Qt::NoPen);
  p.setBrush(withAlpha(INK, 0.05 * bandO));
  p.drawRoundedRect(QRectF(500.0, bandY - 56.0, 920.0, 112.0), 56.0, 56.0);

  for(int i = 0; i < ROW_COUNT; ++i) {
    const Row &row = ROWS[i];
    const qreal t0 = A + 0.4 + i * 1.2;
    const QString letters = QString::fromLatin1(row.l);

    QFont f = brandFont(row.s, QFont::ExtraBold);
    QFontMetricsF fm(f);
    p.setFont(f);

    const qreal gap = row.s * 0.55;
    qreal totalW = 0.0;
    for(const QChar ch : letters) {
      totalW += fm.horizontalAdvance(ch);
    }
    totalW += gap * (letters.length() - 1);

    qreal x = 960.0 - totalW / 2.0;
    for(int j = 0; j < letters.length(); ++j) {
      const QChar ch = letters.at(j);
      const qreal ts = t0 + j * 0.09;
      const qreal o = tween(0.0, 1.0, ts, ts + 0.6, T, Enter);
      const qreal ty = tween(row.s * 0.25, 0.0, ts, ts + 0.9, T, Enter);
      const qreal w = fm.horizontalAdvance(ch);
      if(o > 0.0) {
        p.setOpacity(exitOp * o);
        p.setPen(INK);
        // Vertically centre the glyph on the row's baseline position.
        p.drawText(QPointF(x, row.y + fm.capHeight() / 2.0 + ty),
                   QString(ch));
      }
      x += w + gap;
    }

    // Acuity label on the right.
    const qreal labO = tween(0.0, 1.0, t0 + 0.4, t0 + 1.1, T, Enter);
    if(labO > 0.0) {
      QFont lf = brandFont(24.0, QFont::DemiBold);
      lf.setLetterSpacing(QFont::PercentageSpacing, 108.0);
      p.setFont(lf);
      QFontMetricsF lfm(lf);
      p.setOpacity(exitOp * labO);
      p.setPen(MUTED);
      p.drawText(QPointF(1330.0, row.y + lfm.capHeight() / 2.0),
                 QString::fromLatin1(row.lab));
    }
  }
  p.setOpacity(1.0);
  p.restore();
}

// Draws the progressively revealed "eye" swoosh (two almond-shaped lids) around
// the pupil. The reference clips a logo PNG that is not shipped in the bundle;
// this reconstructs the brand mark as a clean vector that is "drawn" over p:0->1.
void drawSwoosh(QPainter &p, qreal reveal, qreal opacity)
{
  if(reveal <= 0.0 || opacity <= 0.0) {
    return;
  }
  const qreal rx = 168.0;
  const qreal ry = 104.0;
  const QRectF box(DOT_X - rx, DOT_Y - ry, rx * 2.0, ry * 2.0);

  QPen pen(withAlpha(INK, opacity), 12.0, Qt::SolidLine, Qt::RoundCap,
           Qt::RoundJoin);
  p.setPen(pen);
  p.setBrush(Qt::NoBrush);

  // Upper lid: sweeps open from the left corner across the top.
  QPainterPath upper;
  upper.arcMoveTo(box, 180.0);
  upper.arcTo(box, 180.0, -180.0 * reveal);
  p.drawPath(upper);

  // Lower lid: mirror, sweeping along the bottom.
  QPainterPath lower;
  lower.arcMoveTo(box, 0.0);
  lower.arcTo(box, 0.0, -180.0 * reveal);
  p.drawPath(lower);
}

void drawLogo(QPainter &p, qreal T, bool showTagline)
{
  if(T < M - 0.2) {
    return;
  }
  const qreal reveal = tween(0.0, 1.0, M + 0.2, M + 2.4, T, Draw);
  const qreal wordP  = tween(0.0, 1.0, M + 1.9, M + 3.3, T, Draw);
  const qreal wordY  = tween(24.0, 0.0, M + 1.9, M + 3.3, T, Enter);
  const qreal tagO   = tween(0.0, 1.0, M + 3.3, M + 4.6, T, Enter);
  const qreal tagY   = tween(16.0, 0.0, M + 3.3, M + 4.6, T, Enter);
  const qreal outBlur = tween(0.0, 24.0, C, C + 1.6, T, Draw);
  const qreal outOp  = tween(1.0, 0.0, C, C + 1.6, T, Draw);
  const qreal s      = tween(1.0, 1.035, M, C, T, Draw);

  p.save();
  // Approximate the exit "blur" by letting the whole group drift slightly out
  // of scale while it fades, reinforcing the defocus read without a real blur.
  const qreal exitScale = 1.0 + outBlur / 400.0;
  p.setOpacity(outOp);
  p.translate(DOT_X, DOT_Y);
  p.scale(s * exitScale, s * exitScale);
  p.translate(-DOT_X, -DOT_Y);

  drawSwoosh(p, reveal, outOp);

  // Wordmark "OPTILAZ", scaled to exactly fill the reference WORD box and
  // revealed left -> right.
  if(wordP > 0.0) {
    const QString word = "OPTILAZ";
    QFont wf = brandFont(WORD_H, QFont::ExtraBold);
    wf.setLetterSpacing(QFont::PercentageSpacing, 104.0);
    QFontMetricsF wfm(wf);
    const qreal rawW = wfm.horizontalAdvance(word);
    const qreal rawH = wfm.capHeight();
    const qreal sx = rawW > 0.0 ? WORD_W / rawW : 1.0;
    const qreal sy = rawH > 0.0 ? WORD_H / rawH : 1.0;

    p.save();
    // Reveal clip in canvas coordinates.
    p.setClipRect(QRectF(WORD_X, WORD_Y - 40.0 + wordY,
                         WORD_W * wordP, WORD_H + 120.0));
    p.translate(WORD_X, WORD_Y + WORD_H + wordY);
    p.scale(sx, sy);
    p.setFont(wf);
    p.setPen(INK);
    p.drawText(QPointF(0.0, 0.0), word);
    p.restore();
  }

  // Tagline.
  if(showTagline && tagO > 0.0) {
    QFont tf = brandFont(30.0, QFont::DemiBold);
    tf.setLetterSpacing(QFont::PercentageSpacing, 132.0);
    tf.setCapitalization(QFont::AllUppercase);
    QFontMetricsF tfm(tf);
    const QString tag = QString::fromUtf8(TAGLINE);
    p.setFont(tf);
    p.setOpacity(outOp * tagO);
    p.setPen(MUTED);
    const qreal w = tfm.horizontalAdvance(tag.toUpper());
    p.drawText(QPointF((CW - w) / 2.0, TAG_Y + tagY + tfm.capHeight()), tag);
  }

  p.setOpacity(1.0);
  p.restore();
}

void drawDot(QPainter &p, qreal T)
{
  const qreal cs = chartScale(T);
  const qreal cx = 960.0 + (tween(960.0, CURSOR_X, A - 0.3, A + 0.9, T, Draw) - 960.0) * cs;
  const qreal cy = 540.0 + (cursorRawY(T) - 540.0) * cs;

  qreal x, y, r, sc = 1.0, blur = 0.0, op = 1.0;

  if(T < C) {
    const qreal k = tween(0.0, 1.0, A + 7.6, A + 9.2, T, Draw);
    x = cx + (DOT_X - cx) * k;
    y = cy + (DOT_Y - cy) * k;
    r = T < A + 5.0 ? tween(DOT_R, 12.0, A - 0.3, A + 0.9, T, Draw)
                    : tween(12.0, DOT_R, A + 7.6, A + 9.2, T, Draw);
    if(T < A) {
      sc = tween(1.8, 1.0, 0.5, 3.2, T, Enter);
      blur = tween(30.0, 0.0, 0.8, 3.2, T, Draw);
      op = tween(0.0, 1.0, 0.0, 1.2, T, Enter);
    }
    if(T > M) {
      sc = T < M + 0.5 ? tween(1.0, 1.25, M, M + 0.25, T, Enter)
                       : tween(1.25, 1.0, M + 0.25, M + 0.9, T, Pop);
    }
  } else {
    const qreal k = tween(0.0, 1.0, C + 0.6, C + 3.0, T, Draw);
    x = DOT_X + (960.0 - DOT_X) * k;
    y = DOT_Y + (540.0 - DOT_Y) * k;
    r = DOT_R;
    sc = tween(1.0, 1.8, C + 0.6, C + 3.0, T, Draw);
    blur = tween(0.0, 30.0, C + 0.6, C + 3.0, T, Draw);
    op = tween(1.0, 0.0, C + 1.0, C + 3.0, T, Draw);
  }

  // The reference wraps the dot in the logo's slow scale while the logo is up.
  qreal logoS = 1.0;
  if(T > M && T < C + 0.6) {
    logoS = tween(1.0, 1.035, M, C, T, Draw);
  }

  p.save();
  if(logoS != 1.0) {
    p.translate(DOT_X, DOT_Y);
    p.scale(logoS, logoS);
    p.translate(-DOT_X, -DOT_Y);
  }
  const qreal effR = r * sc;
  // Soft yellow glow (the reference's box-shadow).
  blob(p, x, y, effR + r * 1.4, YELLOW, 0.4 * op);
  // Core (defocused when blur > 0).
  softDisc(p, x, y, effR, blur, YELLOW, op);
  p.restore();
}

} // namespace

// ---------------------------------------------------------------------------

ScreenSaver::ScreenSaver(int width, int height, int fps, QWidget *parent) :
  QWidget(parent)
{
  setGeometry(0, 0, width, height);
  // Let all mouse events fall through to the underlying view so the
  // MainWindow event filter stays the single input handler.
  setAttribute(Qt::WA_TransparentForMouseEvents);
  setAutoFillBackground(true);

  if(fps < 5) {
    fps = 5;
  }
  frameSeconds = 1.0 / static_cast<qreal>(fps);
  animTimer.setInterval(1000 / fps);
  connect(&animTimer, &QTimer::timeout, this, &ScreenSaver::step);

  hide();
}

void ScreenSaver::start()
{
  setGeometry(0, 0, parentWidget() ? parentWidget()->width() : width(),
              parentWidget() ? parentWidget()->height() : height());
  t = 0.0;
  raise();
  show();
  animTimer.start();
}

void ScreenSaver::stop()
{
  animTimer.stop();
  hide();
}

void ScreenSaver::step()
{
  t += frameSeconds;
  if(t >= TOTAL) {
    t -= TOTAL;
  }
  update();
}

void ScreenSaver::paintComposition(QPainter &p) const
{
  const qreal T = t;

  drawBackground(p, T);

  // Expanding "focus" rings at screen centre (Enfoque) ...
  drawRing(p, T, 2.4, 960.0, 540.0, 620.0, 2.4, 0.55);
  drawRing(p, T, 3.0, 960.0, 540.0, 620.0, 2.4, 0.55);
  drawRing(p, T, 3.6, 960.0, 540.0, 620.0, 2.4, 0.55);
  // ... and "breathing" rings around the logo (Marca).
  drawRing(p, T, KC + 0.6, DOT_X, DOT_Y, 520.0, 2.8, 0.35);
  drawRing(p, T, KC + 1.6, DOT_X, DOT_Y, 520.0, 2.8, 0.35);

  drawChart(p, T);
  drawLogo(p, T, true);
  drawDot(p, T);
}

void ScreenSaver::paintEvent(QPaintEvent *)
{
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::TextAntialiasing, true);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

  // Fill the letterbox area with the base background colour.
  painter.fillRect(rect(), BG);

  // Map the 1920x1080 canvas onto the widget, preserving aspect ratio and
  // centring (letterbox) so the animation looks correct at any resolution.
  const qreal scale = qMin(width() / CW, height() / CH);
  const qreal offX = (width() - CW * scale) / 2.0;
  const qreal offY = (height() - CH * scale) / 2.0;
  painter.translate(offX, offY);
  painter.scale(scale, scale);
  // Clip to the canvas so blobs/rings never spill into the letterbox bars.
  painter.setClipRect(QRectF(0, 0, CW, CH));

  paintComposition(painter);
}
