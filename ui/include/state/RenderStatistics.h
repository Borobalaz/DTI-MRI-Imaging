#pragma once

#include <deque>

#include <QObject>
#include <QtGlobal>

class RenderStatistics : public QObject
{
  Q_OBJECT
  Q_PROPERTY(double fps READ fps NOTIFY statisticsChanged)
  Q_PROPERTY(double averageFps READ averageFps NOTIFY statisticsChanged)
  Q_PROPERTY(double renderTime READ renderTime NOTIFY statisticsChanged)
  Q_PROPERTY(double averageRenderTime READ averageRenderTime NOTIFY statisticsChanged)

public:
  explicit RenderStatistics(QObject *parent = nullptr);

  double fps() const;
  double averageFps() const;
  double renderTime() const;
  double averageRenderTime() const;

  void reset();
  void recordFrame(double fps, double renderTimeMs, qint64 frameEndNs);

signals:
  void statisticsChanged();

private:
  std::deque<qint64> frameTimesNs;
  std::deque<double> renderTimeWindowMs;
  double renderTimeWindowSumMs = 0.0;

  double fpsValue = 0.0;
  double averageFpsValue = 0.0;
  double renderTimeValue = 0.0;
  double averageRenderTimeValue = 0.0;
};
