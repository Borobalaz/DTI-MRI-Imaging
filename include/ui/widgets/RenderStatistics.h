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
  explicit RenderStatistics(QObject *parent = nullptr)
    : QObject(parent)
  {
  }

  double fps() const { return fpsValue; }
  double averageFps() const { return averageFpsValue; }
  double renderTime() const { return renderTimeValue; }
  double averageRenderTime() const { return averageRenderTimeValue; }

  void reset()
  {
    const bool changed = fpsValue != 0.0 ||
                         averageFpsValue != 0.0 ||
                         renderTimeValue != 0.0 ||
                         averageRenderTimeValue != 0.0 ||
                         !frameTimesNs.empty() ||
                         !renderTimeWindowMs.empty() ||
                         renderTimeWindowSumMs != 0.0;

    frameTimesNs.clear();
    renderTimeWindowMs.clear();
    renderTimeWindowSumMs = 0.0;
    fpsValue = 0.0;
    averageFpsValue = 0.0;
    renderTimeValue = 0.0;
    averageRenderTimeValue = 0.0;

    if (changed)
    {
      emit statisticsChanged();
    }
  }

  void recordFrame(double fps, double renderTimeMs, qint64 frameEndNs)
  {
    fpsValue = fps;
    renderTimeValue = renderTimeMs;

    frameTimesNs.push_back(frameEndNs);
    renderTimeWindowMs.push_back(renderTimeMs);
    renderTimeWindowSumMs += renderTimeMs;

    const qint64 windowStartNs = frameEndNs - 1000000000LL;
    while (!frameTimesNs.empty() && frameTimesNs.front() < windowStartNs)
    {
      renderTimeWindowSumMs -= renderTimeWindowMs.front();
      renderTimeWindowMs.pop_front();
      frameTimesNs.pop_front();
    }

    if (frameTimesNs.size() >= 2)
    {
      const double windowSeconds = static_cast<double>(frameTimesNs.back() - frameTimesNs.front()) / 1e9;
      averageFpsValue = windowSeconds > 1e-6
        ? static_cast<double>(frameTimesNs.size() - 1) / windowSeconds
        : fps;
    }
    else
    {
      averageFpsValue = fps;
    }

    if (!renderTimeWindowMs.empty())
    {
      averageRenderTimeValue = renderTimeWindowSumMs / static_cast<double>(renderTimeWindowMs.size());
    }
    else
    {
      averageRenderTimeValue = renderTimeMs;
    }

    emit statisticsChanged();
  }

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
