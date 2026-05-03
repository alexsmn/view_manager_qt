#include "view_manager_qt_component.h"

#include <QApplication>
#include <QDockWidget>
#include <QMainWindow>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

using testing::ElementsAre;

namespace {

ViewManagerQtComponent::ViewInfo MakeView(
    ViewManagerQtComponent::ViewId id,
    QWidget& widget,
    std::u16string title,
    bool dock = false) {
  return {
      .id = id,
      .widget = &widget,
      .title = std::move(title),
      .state_name = "view_" + std::to_string(id),
      .dock = dock,
  };
}

}  // namespace

TEST(ViewManagerQtComponentTest, AddSplitSaveAndRemovePlainWidgets) {
  QMainWindow main_window;
  ViewManagerQtComponent component{main_window};

  auto widget1 = std::make_unique<QWidget>();
  auto widget2 = std::make_unique<QWidget>();

  std::vector<ViewManagerQtComponent::ViewInfo> views{
      MakeView(1, *widget1, u"One"),
      MakeView(2, *widget2, u"Two"),
  };

  component.AddView(views[0], std::nullopt);
  component.AddView(views[1], views[0].id);
  component.SplitView(views[1].id, /*vertically=*/false);

  auto layout = component.SaveLayout(views);
  EXPECT_EQ(layout.main.type,
            ViewManagerQtComponent::LayoutNode::Type::Split);
  EXPECT_TRUE(layout.main.split_vertical);
  ASSERT_TRUE(layout.main.left);
  ASSERT_TRUE(layout.main.right);
  EXPECT_THAT(layout.main.left->tabs, ElementsAre(views[0].id));
  EXPECT_THAT(layout.main.right->tabs, ElementsAre(views[1].id));

  ASSERT_TRUE(component.RemoveView(views[0].id));
  ASSERT_TRUE(component.RemoveView(views[1].id));
  widget1->setParent(nullptr);
  widget2->setParent(nullptr);
}

TEST(ViewManagerQtComponentTest, OpenLayoutRestoresSplitTabs) {
  QMainWindow main_window;
  ViewManagerQtComponent component{main_window};

  auto widget1 = std::make_unique<QWidget>();
  auto widget2 = std::make_unique<QWidget>();

  std::vector<ViewManagerQtComponent::ViewInfo> views{
      MakeView(1, *widget1, u"One"),
      MakeView(2, *widget2, u"Two"),
  };

  ViewManagerQtComponent::SavedLayout layout;
  layout.main.type = ViewManagerQtComponent::LayoutNode::Type::Split;
  layout.main.split_vertical = false;
  layout.main.split_pos = 25;
  layout.main.left = std::make_unique<ViewManagerQtComponent::LayoutNode>();
  layout.main.left->tabs = {views[0].id};
  layout.main.right = std::make_unique<ViewManagerQtComponent::LayoutNode>();
  layout.main.right->tabs = {views[1].id};

  component.OpenLayout(views, layout);

  auto saved_layout = component.SaveLayout(views);
  EXPECT_EQ(saved_layout.main.type,
            ViewManagerQtComponent::LayoutNode::Type::Split);
  EXPECT_FALSE(saved_layout.main.split_vertical);
  ASSERT_TRUE(saved_layout.main.left);
  ASSERT_TRUE(saved_layout.main.right);
  EXPECT_THAT(saved_layout.main.left->tabs, ElementsAre(views[0].id));
  EXPECT_THAT(saved_layout.main.right->tabs, ElementsAre(views[1].id));

  ASSERT_TRUE(component.RemoveView(views[0].id));
  ASSERT_TRUE(component.RemoveView(views[1].id));
  widget1->setParent(nullptr);
  widget2->setParent(nullptr);
}

TEST(ViewManagerQtComponentTest, SetViewTitleUpdatesTabAndDockTitles) {
  QMainWindow main_window;
  ViewManagerQtComponent component{main_window};

  auto tab_widget = std::make_unique<QWidget>();
  auto* dock_widget = new QWidget;

  auto tab_view = MakeView(1, *tab_widget, u"Tab");
  auto dock_view = MakeView(2, *dock_widget, u"Dock", /*dock=*/true);

  component.AddView(tab_view, std::nullopt);
  component.AddView(dock_view, std::nullopt);

  component.SetViewTitle(tab_view.id, u"New Tab");
  component.SetViewTitle(dock_view.id, u"New Dock");

  auto* tabs = main_window.findChild<DockTabWidget*>();
  ASSERT_NE(tabs, nullptr);
  ASSERT_EQ(tabs->count(), 1);
  EXPECT_EQ(tabs->tabText(0), "New Tab");

  auto docks = main_window.findChildren<QDockWidget*>();
  ASSERT_EQ(docks.size(), 1);
  EXPECT_EQ(docks[0]->windowTitle(), "New Dock");

  ASSERT_TRUE(component.RemoveView(tab_view.id));
  tab_widget->setParent(nullptr);
}

TEST(ViewManagerQtComponentTest, DockCloseInvokesCloseHandler) {
  QMainWindow main_window;
  ViewManagerQtComponent component{main_window};

  auto* widget = new QWidget;
  auto view = MakeView(1, *widget, u"Dock", /*dock=*/true);

  std::vector<ViewManagerQtComponent::ViewId> closed_views;
  component.SetCloseViewHandler([&](ViewManagerQtComponent::ViewId view_id) {
    closed_views.emplace_back(view_id);
  });

  component.AddView(view, std::nullopt);

  auto docks = main_window.findChildren<QDockWidget*>();
  ASSERT_EQ(docks.size(), 1);

  docks[0]->close();
  QApplication::processEvents();

  EXPECT_THAT(closed_views, ElementsAre(view.id));
}
