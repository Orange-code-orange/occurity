/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/***************************************************************************
 *            screensaver.cpp
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
#include "screensaver.h"

#include <QAudioOutput>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QMediaPlayer>
#include <QPalette>
#include <QResizeEvent>
#include <QUrl>
#include <QVideoWidget>

namespace {

// The looping screensaver clip. Installed next to the binary under videos/
// (see CMakeLists install rules); CWD is set to the app dir in main().
const char *VIDEO_NAME = "salvapantallas-optilaz.mp4";

QString findVideo()
{
  const QStringList dirs = {
    QCoreApplication::applicationDirPath() + "/videos",
    QDir::currentPath() + "/videos",
  };
  for(const QString &dir : dirs) {
    const QString path = dir + "/" + VIDEO_NAME;
    if(QFileInfo::exists(path)) {
      return path;
    }
  }
  return QString();
}

} // namespace

ScreenSaver::ScreenSaver(int width, int height, int fps, QWidget *parent) :
  QWidget(parent)
{
  Q_UNUSED(fps);

  setGeometry(0, 0, width, height);
  // Let all mouse events fall through to the underlying view so the
  // MainWindow event filter stays the single input handler.
  setAttribute(Qt::WA_TransparentForMouseEvents);
  setAutoFillBackground(true);

  QPalette pal = palette();
  pal.setColor(QPalette::Window, Qt::black);
  setPalette(pal);

  video = new QVideoWidget(this);
  video->setAttribute(Qt::WA_TransparentForMouseEvents);
  video->setGeometry(0, 0, width, height);
  video->setAspectRatioMode(Qt::KeepAspectRatio);

  // Muted audio output: the clip may carry a silent track, but we never want
  // sound from a screensaver.
  audio = new QAudioOutput(this);
  audio->setMuted(true);

  player = new QMediaPlayer(this);
  player->setAudioOutput(audio);
  player->setVideoOutput(video);

  const QString path = findVideo();
  if(!path.isEmpty()) {
    player->setSource(QUrl::fromLocalFile(path));
  } else {
    qWarning("Screensaver video not found: videos/%s", VIDEO_NAME);
  }

  // Loop endlessly. mediaStatusChanged is the portable way to loop across media
  // backends (more reliable than setLoops on the Pi's FFmpeg backend).
  connect(player, &QMediaPlayer::mediaStatusChanged, this,
          [this](QMediaPlayer::MediaStatus status) {
    if(status == QMediaPlayer::EndOfMedia && isVisible()) {
      player->setPosition(0);
      player->play();
    }
  });

  hide();
}

void ScreenSaver::start()
{
  setGeometry(0, 0, parentWidget() ? parentWidget()->width() : width(),
              parentWidget() ? parentWidget()->height() : height());
  raise();
  show();
  if(player) {
    player->setPosition(0);
    player->play();
  }
}

void ScreenSaver::stop()
{
  if(player) {
    player->stop();
  }
  hide();
}

void ScreenSaver::resizeEvent(QResizeEvent *event)
{
  if(video) {
    video->resize(event->size());
  }
  QWidget::resizeEvent(event);
}
