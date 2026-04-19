#pragma once

#include <QString>

#include "ui/widgets/OpenGLViewportWidget.h"

class DtiVolumeScene;

/**
 * @brief A specialized OpenGL viewport for DTI/diffusion MRI volume rendering.
 * 
 * Inherits from OpenGLViewportWidget to provide OpenGL rendering infrastructure
 * and adds DTI-specific file loading (DWI, bval, bvec) and dataset management.
 */
class DTIViewportWidget : public OpenGLViewportWidget
{
  Q_OBJECT

public:
  explicit DTIViewportWidget(QWidget *parent = nullptr);
  ~DTIViewportWidget() override;

  QString dwiPath() const;
  QString bvalPath() const;
  QString bvecPath() const;

  void setDwiPath(const QString &path);
  void setBvalPath(const QString &path);
  void setBvecPath(const QString &path);

signals:
  void dwiPathChanged();
  void bvalPathChanged();
  void bvecPathChanged();

private:
  void initializeScene() override;

  QString dwiPathValue;
  QString bvalPathValue;
  QString bvecPathValue;
};
