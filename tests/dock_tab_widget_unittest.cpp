#include "dock_tab_widget.h"

#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QMimeData>
#include <QRubberBand>

#include <gtest/gtest.h>

#include <memory>

namespace {

const char kMimeType[] = "action";
const char kMimeData[] = "application/tab-drag";

class TestDockTabWidget : public DockTabWidget {
 public:
  using DockTabWidget::dragEnterEvent;
  using DockTabWidget::dragLeaveEvent;
};

}  // namespace

TEST(DockTabBarTest, CanBeUsedAsTabBar) {
  DockTabBar tab_bar;

  EXPECT_EQ(tab_bar.addTab("One"), 0);
  EXPECT_EQ(tab_bar.tabText(0), "One");
}

TEST(DockTabWidgetTest, ConstructorInstallsDockTabBarAndAcceptsDrops) {
  DockTabWidget tabs;

  EXPECT_TRUE(tabs.acceptDrops());
  EXPECT_NE(dynamic_cast<DockTabBar*>(tabs.tabBar()), nullptr);
}

TEST(DockTabWidgetTest, DragEnterShowsDropRubberBand) {
  TestDockTabWidget tabs;
  auto page = std::make_unique<QWidget>();
  tabs.resize(300, 200);
  tabs.addTab(page.get(), "Page");

  QMimeData mime_data;
  mime_data.setData(kMimeType, kMimeData);
  QDragEnterEvent event{{10, 40}, Qt::MoveAction, &mime_data, Qt::LeftButton,
                        Qt::NoModifier};

  tabs.dragEnterEvent(&event);

  EXPECT_TRUE(event.isAccepted());
  ASSERT_NE(tabs.findChild<QRubberBand*>(), nullptr);

  QDragLeaveEvent leave_event;
  tabs.dragLeaveEvent(&leave_event);

  EXPECT_EQ(tabs.findChild<QRubberBand*>(), nullptr);
  page->setParent(nullptr);
}

TEST(DockTabWidgetTest, DragEnterIgnoresUnknownMimeData) {
  TestDockTabWidget tabs;
  tabs.resize(300, 200);

  QMimeData mime_data;
  mime_data.setData(kMimeType, "unknown");
  QDragEnterEvent event{{10, 40}, Qt::MoveAction, &mime_data, Qt::LeftButton,
                        Qt::NoModifier};

  tabs.dragEnterEvent(&event);

  EXPECT_EQ(tabs.findChild<QRubberBand*>(), nullptr);
}
