/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/***************************************************************************
 *            screensaver.h
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
#ifndef __OCCURITY_SCREENSAVER_H__
#define __OCCURITY_SCREENSAVER_H__

#include <QWidget>
#include <QTimer>

class QPainter;

// Animated OPTILAZ screensaver. It is a faithful C++/QPainter reimplementation
// of the reference web animation (salvapantallas-optilaz): a looping, 29 second
// composition on a virtual 1920x1080 canvas scaled to fit the screen. The
// sequence runs through five scenes -- Enfoque, Agudeza, Mirada, Marca and
// Cierre -- recreating the exact timings and easings of the original. Only
// vectors and text are drawn (no raster image is decoded per frame), keeping it
// Raspberry Pi friendly. Soft "blur"/glow effects are approximated with radial
// gradients so no expensive per-frame gaussian blur is needed.
class ScreenSaver : public QWidget
{
  Q_OBJECT

public:
  ScreenSaver(int width, int height, int fps = 60, QWidget *parent = nullptr);
  void start();
  void stop();

protected:
  void paintEvent(QPaintEvent *event) override;

private slots:
  void step();

private:
  // Draws the full composition for the current time 't' onto 'p'. 'p' is
  // expected to already map the 1920x1080 canvas onto the widget.
  void paintComposition(QPainter &p) const;

  QTimer animTimer;
  // Accumulated animation time in seconds, wrapped to the loop length.
  qreal t = 0.0;
  // Seconds between frames (1 / fps), used to advance 't'.
  qreal frameSeconds = 1.0 / 60.0;
};
#endif/*__OCCURITY_SCREENSAVER_H__*/
