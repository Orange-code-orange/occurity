/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/***************************************************************************
 *            optotypechart.cpp
 *
 *  Tue Jan 11 14:32:00 UTC+1 2022
 *  Copyright 2022 Lars Bisballe
 *  larsbjensen@gmail.com
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

#include "optotypechart.h"
#include "optosymbol.h"

#include <stdio.h>
#include <math.h>

#include <QKeyEvent>
#include <QFileInfo>
#include <QMap>
#include <QMessageBox>
#include <QRandomGenerator>
#include <QPainterPath>

OptotypeChart::OptotypeChart(MainSettings &mainSettings, QObject *parent) :
  AbstractChart(mainSettings, parent)
{
}

OptotypeChart::~OptotypeChart()
{
  for(auto *row: rows) {
    delete row;
  }
}

void OptotypeChart::init()
{
  crowdRect = addRect(0, 0, 10, 10);

  highlightRect = addPath(QPainterPath(), QPen(Qt::NoPen), QBrush());
  highlightRect->hide();
  highlightLine = addLine(0, 0, 1, 0);
  highlightLine->setZValue(2);
  highlightLine->hide();

  sizeItem = new QGraphicsSimpleTextItem("0.0");
  copyrightItem = new QGraphicsSimpleTextItem("");
  QFont font;
  font.setFamily("Arial");
  font.setPixelSize((mainSettings.pxPerArcMin * 5.0) * mainSettings.distanceFactor * 0.2);
  sizeItem->setFont(font);
  font.setPixelSize(25);
  copyrightItem->setFont(font);
  if(QFile::exists("optotypes/copyrights.txt")) {
    QFile copyrightFile("optotypes/copyrights.txt");
    if(copyrightFile.open(QIODevice::ReadOnly)) {
      while(!copyrightFile.atEnd()) {
        QList<QByteArray> copyright = copyrightFile.readLine().split(';');
        if(!copyright.isEmpty() && optotype == copyright.first()) {
          copyrightItem->setText(copyright.last().trimmed());
          break;
        }
      }
      copyrightFile.close();
    }
  }
  titleItem->setText(objectName());

  addItem(sizeItem);
  sizeItem->hide();
  addItem(titleItem);
  addItem(copyrightItem);
  copyrightItem->setPos((mainSettings.width / 2.0) - (copyrightItem->boundingRect().width() / 2.0), mainSettings.height - 35);

  animTimer = new QTimer(this);
  animTimer->setInterval(16);
  connect(animTimer, &QTimer::timeout, this, &OptotypeChart::animStep);

  setSize(startSize);
}

void OptotypeChart::keyPressEvent(QKeyEvent *event)
{
  bool navigationChanged = false;

  if(event->key() == Qt::Key_Up ||
     event->key() == Qt::Key_Down ||
     event->key() == Qt::Key_PageUp ||
     event->key() == Qt::Key_PageDown) {
    animStartY.resize(rows.length());
    for(int a = 0; a < rows.length(); ++a) {
      animStartY[a] = rows.at(a)->getLetters()->y();
    }
    if(event->key() == Qt::Key_Up) {
      currentRowIdx--;
      if(currentRowIdx < 0) {
        currentRowIdx = 0;
      }
    } else if(event->key() == Qt::Key_Down) {
      currentRowIdx++;
      if(currentRowIdx > rows.length() - 1) {
        currentRowIdx = rows.length() - 1;
      }
    } else if(event->key() == Qt::Key_PageUp) {
      currentRowIdx -= mainSettings.rowSkipDelta;
      if(currentRowIdx < 0) {
        currentRowIdx = 0;
      }
    } else if(event->key() == Qt::Key_PageDown) {
      currentRowIdx += mainSettings.rowSkipDelta;
      if(currentRowIdx > rows.length() - 1) {
        currentRowIdx = rows.length() - 1;
      }
    }
    positionReset();
    animTargetY = computeTargetY();
    animating = true;
    animElapsed.start();
    if(!animTimer->isActive()) animTimer->start(16);
    navigationChanged = true;
  } else if(event->key() == Qt::Key_C) {
    highlightVisible = !highlightVisible;
  } else if(event->key() == Qt::Key_M) {
    mainSettings.single = !mainSettings.single;
  } else if(event->key() == Qt::Key_R) {
    QList<QPointF> availableCoords;
    for(const auto &child: rows.at(currentRowIdx)->getLetters()->childItems()) {
      availableCoords.append(child->pos());
    }
    for(const auto &child: rows.at(currentRowIdx)->getLetters()->childItems()) {
      int chosen = QRandomGenerator::global()->bounded(availableCoords.length());
      child->setPos(availableCoords.at(chosen));
      availableCoords.removeAt(chosen);
    }
  }

  if(!navigationChanged) {
    updateAll();
  }
}

void OptotypeChart::setOptotype(const QString &optotype)
{
  this->optotype = optotype;
}

QString OptotypeChart::getOptotype()
{
  return optotype;
}

void OptotypeChart::setCrowdingSpan(const double &crowdingSpan)
{
  this->crowdingSpan = crowdingSpan;
}

double OptotypeChart::getCrowdingSpan()
{
  return crowdingSpan;
}

void OptotypeChart::addRow(const QString &size, const QString &caption, const QString &rowChars)
{
  /*
    Don't do any resizing / scaling here. It has to be done in updateAll(), otherwise it
    won't be re-scaled when preferences are changed.
    Here we simply load all SVG's into QGraphicsSvgItems, move them into place and add them
    to QGraphicsItemGroups.
  */
  OptoSymbol *svgSpace = new OptoSymbol(mainSettings.optotypesFolder + "/" + optotype + "/_.svg",
                                        fadeTimings,
                                        fadeLevels);
  spaceWidth = svgSpace->boundingRect().width();

  QGraphicsItemGroup *layer = new QGraphicsItemGroup;
  addItem(layer);

  int curX = 0;
  QList<QString> letters;
  if(rowChars.contains(";")) {
    for(const auto &letter: rowChars.split(";")) {
      letters.append(letter);
    }
  } else {
    for(const auto &letter: rowChars) {
      letters.append(letter);
    }
  }
  for(const auto &letter: letters) {
    QString svgFilename = mainSettings.optotypesFolder + "/" + optotype + "/" + letter + ".svg";
    if(QFileInfo::exists(svgFilename)) {
      OptoSymbol *svgLetter = new OptoSymbol(svgFilename, fadeTimings, fadeLevels);
      svgLetter->setX(curX);
      svgLetter->setData(0, svgLetter->pos());
      layer->addToGroup(svgLetter);
      curX += svgLetter->boundingRect().width() + spaceWidth;
      svgLetter->hide();
    } else {
      QMessageBox::warning(nullptr, tr("File not found"), tr("File '") + svgFilename + tr("' not found. Please correct this."));
    }
  }

  OptotypeRow *optotypeRow = new OptotypeRow(size, caption, layer);

  rows.append(optotypeRow);

  QGraphicsSimpleTextItem *leftLabel = new QGraphicsSimpleTextItem();
  QGraphicsSimpleTextItem *rightLabel = new QGraphicsSimpleTextItem();
  leftLabel->setZValue(1);
  rightLabel->setZValue(1);
  addItem(leftLabel);
  addItem(rightLabel);
  leftLabel->hide();
  rightLabel->hide();
  rowLeftLabels.append(leftLabel);
  rowRightLabels.append(rightLabel);
}

QString OptotypeChart::snellenFraction(double decimal)
{
  if(decimal <= 0) return "";
  static const QMap<int, QString> snellenMap = {
    {50,   "20/400"},  {63,   "20/320"},  {67,   "20/320"},
    {80,   "20/250"},  {100,  "20/200"},  {125,  "20/160"},
    {160,  "20/125"},  {200,  "20/100"},  {250,  "20/80"},
    {320,  "20/63"},   {400,  "20/50"},   {500,  "20/40"},
    {630,  "20/32"},   {800,  "20/25"},   {1000, "20/20"},
    {1250, "20/16"},   {1600, "20/12.5"}, {2000, "20/10"},
  };
  int key = qRound(decimal * 1000);
  if(snellenMap.contains(key)) {
    return snellenMap[key];
  }
  double denom = 20.0 / decimal;
  double denomHalf = qRound(denom * 2) / 2.0;
  return "20/" + (denomHalf == qFloor(denomHalf)
                  ? QString::number((int)denomHalf)
                  : QString::number(denomHalf, 'f', 1));
}

void OptotypeChart::updateAll()
{
  const double labelMargin = 130.0;
  const int labelFontSize = 30;
  const double rowGap = mainSettings.height * 0.13;

  // Step A: Apply correct scale to each row and center horizontally
  for(int a = 0; a < rows.length(); ++a) {
    double decimal = rows.at(a)->getSize().toDouble();
    if(decimal <= 0) continue;
    double scale = (mainSettings.pxPerArcMin / ((10.0 * decimal) * 100.0)) * mainSettings.distanceFactor;
    rows.at(a)->getLetters()->setScale(scale);
    double rowWidth = rows.at(a)->getLetters()->mapRectToScene(rows.at(a)->getLetters()->boundingRect()).width();
    rows.at(a)->getLetters()->setX((mainSettings.width / 2.0) - (rowWidth / 2.0));
  }

  // Step B: Vertical positioning — current row centered, previous above, next below
  if(!animating) {
    double currentRowHeight = rows.at(currentRowIdx)->getLetters()->mapRectToScene(
      rows.at(currentRowIdx)->getLetters()->boundingRect()).height();
    double currentY = (mainSettings.height / 2.0) - (currentRowHeight / 2.0);
    rows.at(currentRowIdx)->getLetters()->setY(currentY);

    double topY = currentY;
    for(int a = currentRowIdx - 1; a >= 0; --a) {
      double decimal = rows.at(a)->getSize().toDouble();
      if(decimal <= 0) continue;
      double rowHeight = rows.at(a)->getLetters()->mapRectToScene(rows.at(a)->getLetters()->boundingRect()).height();
      topY -= rowHeight + rowGap;
      rows.at(a)->getLetters()->setY(topY);
    }

    double bottomY = currentY + currentRowHeight;
    for(int a = currentRowIdx + 1; a < rows.length(); ++a) {
      double decimal = rows.at(a)->getSize().toDouble();
      if(decimal <= 0) continue;
      double rowHeight = rows.at(a)->getLetters()->mapRectToScene(rows.at(a)->getLetters()->boundingRect()).height();
      rows.at(a)->getLetters()->setY(bottomY + rowGap);
      bottomY += rowHeight + rowGap;
    }
  }

  // Step C: Show letters; apply single/crowding effects only to the active row
  for(int a = 0; a < rows.length(); ++a) {
    if(a == currentRowIdx) {
      double decimalSize = rows.at(a)->getSize().toDouble();
      double arcMinScaled = ((mainSettings.pxPerArcMin / ((10.0 * decimalSize) * 100.0)) * 100.0) * mainSettings.distanceFactor;
      double spaceWidthScaled = (spaceWidth / 100.0) * arcMinScaled;

      rows.at(a)->getLetters()->setX((mainSettings.width / 2.0) - (rows.at(a)->getLetters()->mapRectToScene(rows.at(a)->getLetters()->boundingRect()).width() / 2.0));

      for(QGraphicsItem *graphicsItem: rows.at(a)->getLetters()->childItems()) {
        static_cast<OptoSymbol *>(graphicsItem)->fadeIn();
      }

      // Show / hide optotypes depending on screen edges or single mode
      for(QGraphicsItem *graphicsItem: rows.at(a)->getLetters()->childItems()) {
        OptoSymbol *optoSymbol = static_cast<OptoSymbol *>(graphicsItem);
        if(mainSettings.single) {
          if(rows.at(a)->getLetters()->mapToScene(optoSymbol->pos()).x() + (rows.at(a)->getLetters()->mapRectToScene(optoSymbol->boundingRect()).width() / 2.0) >= mainSettings.width / 2.0 - 2 &&
             rows.at(a)->getLetters()->mapToScene(optoSymbol->pos()).x() + (rows.at(a)->getLetters()->mapRectToScene(optoSymbol->boundingRect()).width() / 2.0) <= mainSettings.width / 2.0 + 2) {
            optoSymbol->fadeIn();
          } else {
            optoSymbol->fadeInOut();
          }
        } else {
          if(rows.at(a)->getLetters()->mapToScene(optoSymbol->pos()).x() < labelMargin ||
             rows.at(a)->getLetters()->mapToScene(optoSymbol->pos()).x() + rows.at(a)->getLetters()->mapRectToScene(optoSymbol->boundingRect()).width() > mainSettings.width - labelMargin) {
            optoSymbol->fadeInOut();
          } else {
            optoSymbol->fadeIn();
          }
        }
      }

      crowdRect->hide();
    } else {
      for(QGraphicsItem *graphicsItem: rows.at(a)->getLetters()->childItems()) {
        OptoSymbol *optoSymbol = static_cast<OptoSymbol *>(graphicsItem);
        double sx = rows.at(a)->getLetters()->mapToScene(optoSymbol->pos()).x();
        double sw = rows.at(a)->getLetters()->mapRectToScene(optoSymbol->boundingRect()).width();
        if(sx < labelMargin || sx + sw > mainSettings.width - labelMargin) {
          optoSymbol->fadeInOut();
        } else {
          optoSymbol->fadeIn();
        }
      }
    }
  }

  // Step D: Green highlight — fixed width, rounded corners, progressive gradient, bottom line
  {
    if(highlightVisible) {
      QRectF cr = rows.at(currentRowIdx)->getLetters()->mapRectToScene(
        rows.at(currentRowIdx)->getLetters()->boundingRect());
      const double vPad = 14.0;
      const double fixedX = labelMargin;
      const double fixedW = mainSettings.width - 2.0 * labelMargin;
      const double rectH = cr.height() + 2.0 * vPad;
      const double rectY = cr.y() - vPad;
      const double radius = rectH * 0.35;

      QColor col(mainSettings.hexGreen);
      QColor light = col.lighter(145);
      auto a = [&](int alpha) { QColor c = light; c.setAlpha(alpha); return c; };

      QLinearGradient gradient(fixedX, 0, fixedX + fixedW, 0);
      gradient.setColorAt(0.00, a(255));
      gradient.setColorAt(0.18, a(210));
      gradient.setColorAt(0.38, a(80));
      gradient.setColorAt(0.50, a(5));
      gradient.setColorAt(0.62, a(80));
      gradient.setColorAt(0.82, a(210));
      gradient.setColorAt(1.00, a(255));

      QPainterPath path;
      path.addRoundedRect(fixedX, rectY, fixedW, rectH, radius, radius);
      highlightRect->setPath(path);
      highlightRect->setBrush(QBrush(gradient));
      highlightRect->setPen(QPen(Qt::NoPen));
      highlightRect->stackBefore(rows.at(currentRowIdx)->getLetters());
      highlightRect->show();

      const double lineW = 5.0;
      const double lineY = rectY + rectH - lineW / 2.0 - 1.0;
      QPen linePen(light, lineW);
      linePen.setCapStyle(Qt::RoundCap);
      highlightLine->setPen(linePen);
      highlightLine->setLine(fixedX + radius, lineY,
                             fixedX + fixedW - radius, lineY);
      highlightLine->show();
    } else {
      highlightRect->hide();
      highlightLine->hide();
    }
  }

  // Step E: Per-row labels (decimal on left, Snellen fraction on right)
  {
    QFont labelFont;
    labelFont.setFamily("Arial");
    labelFont.setPixelSize(labelFontSize);
    for(int a = 0; a < rows.length() && a < rowLeftLabels.length(); ++a) {
      double decimal = rows.at(a)->getSize().toDouble();
      if(decimal <= 0) continue;
      QRectF rowRect = rows.at(a)->getLetters()->mapRectToScene(
        rows.at(a)->getLetters()->boundingRect());

      // Left label: decimal visual acuity
      rowLeftLabels.at(a)->setFont(labelFont);
      rowLeftLabels.at(a)->setText(rows.at(a)->getSize());
      rowLeftLabels.at(a)->setBrush(QColor(mainSettings.hexRed));
      rowLeftLabels.at(a)->setX(10);
      rowLeftLabels.at(a)->setY(rowRect.y() + (rowRect.height() - rowLeftLabels.at(a)->boundingRect().height()) / 2.0);
      rowLeftLabels.at(a)->show();

      // Right label: Snellen fraction (20/XX)
      QString fraction = (mainSettings.useRowCaptions && !rows.at(a)->getCaption().isEmpty())
                          ? rows.at(a)->getCaption()
                          : snellenFraction(decimal);
      rowRightLabels.at(a)->setFont(labelFont);
      rowRightLabels.at(a)->setText(fraction);
      rowRightLabels.at(a)->setBrush(QColor(mainSettings.hexRed));
      QRectF rBounds = rowRightLabels.at(a)->boundingRect();
      rowRightLabels.at(a)->setX(sceneRect().right() - rBounds.width() - 10);
      rowRightLabels.at(a)->setY(rowRect.y() + (rowRect.height() - rBounds.height()) / 2.0);
      rowRightLabels.at(a)->show();
    }
  }

  // Step F: Update title position
  {
    QFont font;
    font.setFamily("Arial");
    font.setPixelSize(mainSettings.pxPerArcMin * mainSettings.distanceFactor * 1.5);
    titleItem->setFont(font);
    titleItem->setX(mainSettings.width - titleItem->boundingRect().width() - 10);
    titleItem->setY(mainSettings.height - titleItem->boundingRect().height() - 10);
  }

  // Update entire screen area
  update(0, 0, mainSettings.width, mainSettings.height);
}

void OptotypeChart::setStartSize(const QString &startSize)
{
  this->startSize = startSize;
}

void OptotypeChart::setFadeTimings(const QString &fadeTimings)
{
  this->fadeTimings = fadeTimings;
}

void OptotypeChart::setFadeLevels(const QString &fadeLevels)
{
  this->fadeLevels = fadeLevels;
}

void OptotypeChart::resetAll()
{
  setSize(startSize);
}

void OptotypeChart::setSize(const QString &sizeStr)
{
  for(int a = 0; a < rows.length(); ++a) {
    if(rows.at(a)->getSize() == sizeStr) {
      currentRowIdx = a;
      break;
    }
  }
  positionReset();
  updateAll();
}

QString OptotypeChart::getSize()
{
  return rows.at(currentRowIdx)->getSize();
}

void OptotypeChart::positionReset()
{
  // Reset positions for all optotype symbols in row in case they've been randomized
  for(const auto &child: rows.at(currentRowIdx)->getLetters()->childItems()) {
    child->setPos(child->data(0).toPointF());
  }
}

void OptotypeChart::hideFromList()
{
  for(auto *item: hideList) {
    item->hide();
  }
  hideList.clear();
}

QList<double> OptotypeChart::computeTargetY() const
{
  const double rowGap = mainSettings.height * 0.13;
  QList<double> result(rows.length(), 0.0);

  double currentRowHeight = rows.at(currentRowIdx)->getLetters()->mapRectToScene(
    rows.at(currentRowIdx)->getLetters()->boundingRect()).height();
  double currentY = (mainSettings.height / 2.0) - (currentRowHeight / 2.0);
  result[currentRowIdx] = currentY;

  double topY = currentY;
  for(int a = currentRowIdx - 1; a >= 0; --a) {
    double rowHeight = rows.at(a)->getLetters()->mapRectToScene(
      rows.at(a)->getLetters()->boundingRect()).height();
    topY -= rowHeight + rowGap;
    result[a] = topY;
  }

  double bottomY = currentY + currentRowHeight;
  for(int a = currentRowIdx + 1; a < rows.length(); ++a) {
    double rowHeight = rows.at(a)->getLetters()->mapRectToScene(
      rows.at(a)->getLetters()->boundingRect()).height();
    result[a] = bottomY + rowGap;
    bottomY += rowHeight + rowGap;
  }

  return result;
}

void OptotypeChart::animStep()
{
  double t = qMin(1.0, (double)animElapsed.elapsed() / ANIM_DURATION_MS);
  // ease in-out cubic
  double et = t < 0.5 ? 4.0 * t * t * t : 1.0 - 4.0 * (1.0 - t) * (1.0 - t) * (1.0 - t) / 2.0;

  for(int a = 0; a < rows.length() && a < animStartY.length() && a < animTargetY.length(); ++a) {
    rows.at(a)->getLetters()->setY(animStartY.at(a) + (animTargetY.at(a) - animStartY.at(a)) * et);
  }

  if(t >= 1.0) {
    animTimer->stop();
    animating = false;
  }

  updateAll();
}
