/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/***************************************************************************
 *            screensaver.h
 *
 *  Looping OPTILAZ video screensaver overlay
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

class QMediaPlayer;
class QAudioOutput;
class QVideoWidget;
class QResizeEvent;

// OPTILAZ screensaver. Plays the branded OPTILAZ video
// (videos/salvapantallas-optilaz.mp4) full screen in an endless loop. Hardware
// video decoding keeps it smooth on the Raspberry Pi, where the previous
// per-frame QPainter animation stuttered. The widget is transparent to mouse
// events so the MainWindow event filter stays the single input handler and can
// dismiss the screensaver on any activity.
class ScreenSaver : public QWidget
{
  Q_OBJECT

public:
  // 'fps' is kept for call-site compatibility but is unused: playback runs at
  // the video's own frame rate.
  ScreenSaver(int width, int height, int fps = 60, QWidget *parent = nullptr);
  void start();
  void stop();

protected:
  void resizeEvent(QResizeEvent *event) override;

private:
  QMediaPlayer *player = nullptr;
  QAudioOutput *audio = nullptr;
  QVideoWidget *video = nullptr;
};
#endif/*__OCCURITY_SCREENSAVER_H__*/
