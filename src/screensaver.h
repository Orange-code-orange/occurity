/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/***************************************************************************
 *            screensaver.h
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
#ifndef __OCCURITY_SCREENSAVER_H__
#define __OCCURITY_SCREENSAVER_H__

#include <QWidget>
#include <QTimer>
#include <QPointF>

// Lightweight animated screensaver recreating the OPTILAZ logo as vector
// graphics (no raster image is decoded per frame, keeping it Raspberry Pi
// friendly). The eye "rings" (two navy arcs) counter-rotate, the yellow pupil
// pulses and the "OPTILAZ" letters do a continuous vertical wave. The whole
// logo group slowly drifts and bounces off the screen edges (DVD style) to
// avoid screen burn-in.
class ScreenSaver : public QWidget
{
  Q_OBJECT

public:
  ScreenSaver(int width, int height, int fps = 30, QWidget *parent = nullptr);
  void start();
  void stop();

protected:
  void paintEvent(QPaintEvent *event) override;

private slots:
  void step();

private:
  QTimer animTimer;
  // Accumulated animation time in seconds.
  qreal t = 0.0;
  // Seconds between frames (1 / fps), used to advance 't'.
  qreal frameSeconds = 1.0 / 30.0;

  // Position (top-left of the logo group) and velocity for the drifting /
  // bouncing movement.
  QPointF groupPos;
  QPointF groupVel;
  QSizeF groupSize;
};
#endif/*__OCCURITY_SCREENSAVER_H__*/
