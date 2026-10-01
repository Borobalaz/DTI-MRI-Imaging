#include "state/RenderStatistics.h"

RenderStatistics::RenderStatistics(QObject *parent)
  : QObject(parent)
{
}

double RenderStatistics::fps() const { return fpsValue; }
double RenderStatistics::averageFps() const { return averageFpsValue; }
double RenderStatistics::renderTime() const { return renderTimeValue; }
double RenderStatistics::averageRenderTime() const { return averageRenderTimeValue; }

void RenderStatistics::reset()
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

void RenderStatistics::recordFrame(double fps, double renderTimeMs, qint64 frameEndNs)
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