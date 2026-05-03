#pragma once

#include <QTabBar>
#include <QTabWidget>
#include <memory>

class QRubberBand;

class DockTabBar : public QTabBar {
 public:
 protected:
  virtual void mousePressEvent(QMouseEvent* event) override;
  virtual void mouseMoveEvent(QMouseEvent* event) override;

 private:
  QPoint drag_start_pos_;
};

class DockTabWidget : public QTabWidget {
  Q_OBJECT

 public:
  explicit DockTabWidget(QWidget* parent = nullptr);

  enum class DropSide { Left, Top, Right, Bottom, Center };

 signals:
  void tabDropped(DockTabWidget& source, int source_index, DropSide side);

 protected:
  virtual void dragEnterEvent(QDragEnterEvent* event) override;
  virtual void dragMoveEvent(QDragMoveEvent* event) override;
  virtual void dragLeaveEvent(QDragLeaveEvent* event) override;
  virtual void dropEvent(QDropEvent* event) override;

 private:
  std::unique_ptr<QRubberBand> rubber_band_;
};
