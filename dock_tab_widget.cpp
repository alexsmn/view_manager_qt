#include "dock_tab_widget.h"

#include <QDrag>
#include <QMimeData>
#include <QMouseEvent>
#include <QRubberBand>
#include <cassert>

namespace {

const int kDragStartOffset = 5;

const char kMimeType[] = "action";
const char kMimeData[] = "application/tab-drag";

QRect CalcDropRect(const QRect& rect,
                   DockTabWidget::DropSide side,
                   int tab_bar_height) {
  switch (side) {
    case DockTabWidget::DropSide::Left:
      return {rect.left(), rect.top(), rect.width() / 3, rect.height()};
    case DockTabWidget::DropSide::Top:
      return {rect.left(), rect.top(), rect.width(), rect.height() / 3};
    case DockTabWidget::DropSide::Right:
      return {rect.right() - rect.width() / 3, rect.top(), rect.width() / 3,
              rect.height()};
    case DockTabWidget::DropSide::Bottom:
      return {rect.left(), rect.bottom() - rect.height() / 3, rect.width(),
              rect.height() / 3};
    case DockTabWidget::DropSide::Center:
      return {rect.left(), rect.top() + tab_bar_height, rect.width(),
              rect.height() - tab_bar_height};
    default:
      assert(false);
      return rect;
  }
}

DockTabWidget::DropSide CalcDropSide(const QRect& rect,
                                     QPoint point,
                                     int tab_bar_height) {
  const DockTabWidget::DropSide kSides[] = {
      DockTabWidget::DropSide::Left, DockTabWidget::DropSide::Top,
      DockTabWidget::DropSide::Right, DockTabWidget::DropSide::Bottom};
  for (auto side : kSides) {
    if (CalcDropRect(rect, side, tab_bar_height).contains(point))
      return side;
  }
  return DockTabWidget::DropSide::Center;
}

}  // namespace

// DockTabBar

void DockTabBar::mousePressEvent(QMouseEvent* event) {
  QTabBar::mousePressEvent(event);
}

void DockTabBar::mouseMoveEvent(QMouseEvent* event) {
  QTabBar::mouseMoveEvent(event);

  if (currentIndex() != -1 && (event->buttons() & Qt::LeftButton) &&
      !rect().contains(event->pos())) {
    QMouseEvent finish_move_event{QEvent::MouseMove, event->pos(), Qt::NoButton,
                                  Qt::NoButton, Qt::NoModifier};
    QTabBar::mouseMoveEvent(&finish_move_event);

    auto* drag = new QDrag{this};
    auto* data = new QMimeData;
    data->setData(kMimeType, kMimeData);
    drag->setMimeData(data);
    drag->exec();
  }
}

// DockTabWidget

DockTabWidget::DockTabWidget(QWidget* parent) : QTabWidget{parent} {
  setTabBar(new DockTabBar);

  setAcceptDrops(true);
}

void DockTabWidget::dragEnterEvent(QDragEnterEvent* event) {
  QTabWidget::dragEnterEvent(event);

  if (event->mimeData() && event->mimeData()->data(kMimeType) == kMimeData) {
    assert(!rubber_band_);
    rubber_band_ = std::make_unique<QRubberBand>(QRubberBand::Rectangle, this);

    auto rect = this->rect();
    int tab_bar_height = tabBar()->height();
    auto side = CalcDropSide(rect, event->pos(), tab_bar_height);
    rubber_band_->setGeometry(CalcDropRect(rect, side, tab_bar_height));

    rubber_band_->show();

    event->acceptProposedAction();
  }
}

void DockTabWidget::dragMoveEvent(QDragMoveEvent* event) {
  auto rect = this->rect();
  int tab_bar_height = tabBar()->height();
  auto side = CalcDropSide(rect, event->pos(), tab_bar_height);
  rubber_band_->setGeometry(CalcDropRect(rect, side, tab_bar_height));

  QTabWidget::dragMoveEvent(event);
}

void DockTabWidget::dragLeaveEvent(QDragLeaveEvent* event) {
  rubber_band_.reset();

  QTabWidget::dragLeaveEvent(event);
}

void DockTabWidget::dropEvent(QDropEvent* event) {
  rubber_band_.reset();

  auto rect = this->rect();
  int tab_bar_height = tabBar()->height();
  auto side = CalcDropSide(rect, event->pos(), tab_bar_height);

  auto* source_tab_bar = static_cast<QTabBar*>(event->source());
  auto* source_tab_widget =
      static_cast<DockTabWidget*>(source_tab_bar->parentWidget());
  assert(source_tab_widget);

  int source_tab_index = source_tab_widget->currentIndex();
  assert(source_tab_index != -1);

  emit tabDropped(*source_tab_widget, source_tab_index, side);

  QTabWidget::dropEvent(event);
}
