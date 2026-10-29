/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/***************************************************************************
 *            screensaver.cpp
 *
 *  Lightweight animated OPTILAZ screensaver overlay
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

#include <cmath>

// Brand colours. On the black screensaver background the navy blue of the logo
// would be too dark, so a brighter blue is used for the eye arcs while keeping
// the recognisable yellow pupil.
static const QColor ARC_COLOR(70, 120, 230);     // bright brand-ish blue
static const QColor PUPIL_COLOR(239, 212, 78);   // OPTILAZ yellow (#EFD44E)

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

  // The logo (an eye) occupies a fraction of the screen width. A square
  // footprint is used so the rotating arcs never leave the group box.
  qreal eyeW = width * 0.50;
  groupSize = QSizeF(eyeW, eyeW);

  // Start centred, drifting diagonally at a calm pace (pixels per second).
  groupPos = QPointF((width - groupSize.width()) / 2.0,
                     (height - groupSize.height()) / 2.0);
  groupVel = QPointF(55.0, 41.0);

  hide();
}

void ScreenSaver::start()
{
  setGeometry(0, 0, parentWidget() ? parentWidget()->width() : width(),
              parentWidget() ? parentWidget()->height() : height());
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

  // Advance the drifting group and bounce off the window edges.
  groupPos += groupVel * frameSeconds;

  const qreal maxX = width() - groupSize.width();
  const qreal maxY = height() - groupSize.height();

  if(groupPos.x() < 0.0) {
    groupPos.setX(0.0);
    groupVel.setX(-groupVel.x());
  } else if(groupPos.x() > maxX) {
    groupPos.setX(maxX);
    groupVel.setX(-groupVel.x());
  }
  if(groupPos.y() < 0.0) {
    groupPos.setY(0.0);
    groupVel.setY(-groupVel.y());
  } else if(groupPos.y() > maxY) {
    groupPos.setY(maxY);
    groupVel.setY(-groupVel.y());
  }

  update();
}

void ScreenSaver::paintEvent(QPaintEvent *)
{
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);

  // Background
  painter.fillRect(rect(), Qt::black);

  const qreal eyeW = groupSize.width();
  const qreal eyeH = eyeW * 0.45;

  // --- Eye with counter-rotating "ring" arcs and pulsing pupil ---
  const qreal cx = groupPos.x() + eyeW / 2.0;
  const qreal cy = groupPos.y() + eyeW / 2.0;
  const qreal thick = eyeH * 0.14;

  // Arc bounding rect, inset so the thick stroke stays inside the group box.
  const QRectF arcRect(-(eyeW - thick) / 2.0, -(eyeH - thick) / 2.0,
                       eyeW - thick, eyeH - thick);

  QPen arcPen(ARC_COLOR, thick, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

  // Rotation angle of the rings (degrees). Slow and continuous.
  const qreal angle = t * 20.0;

  painter.save();
  painter.translate(cx, cy);

  painter.setBrush(Qt::NoBrush);
  painter.setPen(arcPen);

  // Top arc rotates one way...
  painter.save();
  painter.rotate(angle);
  QPainterPath topArc;
  topArc.arcMoveTo(arcRect, 40.0);
  topArc.arcTo(arcRect, 40.0, 100.0);
  painter.drawPath(topArc);
  painter.restore();

  // ...bottom arc rotates the other way.
  painter.save();
  painter.rotate(-angle);
  QPainterPath bottomArc;
  bottomArc.arcMoveTo(arcRect, 220.0);
  bottomArc.arcTo(arcRect, 220.0, 100.0);
  painter.drawPath(bottomArc);
  painter.restore();

  // Pulsing yellow pupil in the centre.
  const qreal pupilBase = eyeH * 0.16;
  const qreal pupilR = pupilBase * (1.0 + 0.15 * std::sin(t * 2.2));
  painter.setPen(Qt::NoPen);
  painter.setBrush(PUPIL_COLOR);
  painter.drawEllipse(QPointF(0.0, 0.0), pupilR, pupilR);

  painter.restore();
}
