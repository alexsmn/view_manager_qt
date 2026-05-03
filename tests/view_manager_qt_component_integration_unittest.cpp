#include "view_manager_qt_component.h"

#include <QApplication>
#include <QDir>
#include <QDockWidget>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QTabBar>
#include <QTableWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

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
    bool dock = false,
    bool dock_bottom = false) {
  widget.setObjectName(QString{"view_%1_widget"}.arg(id));
  widget.setFocusPolicy(Qt::StrongFocus);

  return {
      .id = id,
      .widget = &widget,
      .title = std::move(title),
      .state_name = "view_" + std::to_string(id),
      .dock = dock,
      .dock_bottom = dock_bottom,
  };
}

std::unique_ptr<QListWidget> CreateEventList() {
  auto list = std::make_unique<QListWidget>();
  list->addItems({"Pump station online", "Operator acknowledged alarm",
                  "Pressure normalized", "Backup generator ready"});
  list->setCurrentRow(1);
  return list;
}

std::unique_ptr<QTreeWidget> CreateAssetTree() {
  auto tree = std::make_unique<QTreeWidget>();
  tree->setHeaderLabels({"Asset", "State"});

  auto* station = new QTreeWidgetItem{tree.get(), {"Station A", "Running"}};
  new QTreeWidgetItem{station, {"Pump 1", "Nominal"}};
  new QTreeWidgetItem{station, {"Pump 2", "Standby"}};

  auto* remote = new QTreeWidgetItem{tree.get(), {"Remote Site", "Warning"}};
  new QTreeWidgetItem{remote, {"RTU", "Connected"}};
  new QTreeWidgetItem{remote, {"Battery", "Low"}};

  tree->expandAll();
  tree->setCurrentItem(remote->child(1));
  return tree;
}

std::unique_ptr<QTableWidget> CreateTrendTable() {
  auto table = std::make_unique<QTableWidget>(4, 3);
  table->setHorizontalHeaderLabels({"Signal", "Value", "Quality"});
  const QString rows[][3] = {
      {"Flow", "124.6 m3/h", "Good"},
      {"Pressure", "7.8 bar", "Good"},
      {"Level", "62 %", "Good"},
      {"Temperature", "43 C", "Good"},
  };
  for (int row = 0; row < 4; ++row) {
    for (int column = 0; column < 3; ++column)
      table->setItem(row, column, new QTableWidgetItem{rows[row][column]});
  }
  table->resizeColumnsToContents();
  table->setCurrentCell(0, 1);
  return table;
}

std::unique_ptr<QWidget> CreatePropertiesView() {
  auto widget = std::make_unique<QWidget>();
  auto* layout = new QVBoxLayout{widget.get()};
  layout->setContentsMargins(8, 8, 8, 8);

  auto* title = new QLabel{"Selected Asset"};
  title->setStyleSheet("font-weight: 600");
  layout->addWidget(title);

  auto* properties = new QTableWidget{4, 2};
  properties->setHorizontalHeaderLabels({"Property", "Value"});
  const QString rows[][2] = {
      {"Name", "Remote Site"},
      {"Mode", "Automatic"},
      {"Priority", "High"},
      {"Owner", "Operations"},
  };
  for (int row = 0; row < 4; ++row) {
    for (int column = 0; column < 2; ++column)
      properties->setItem(row, column, new QTableWidgetItem{rows[row][column]});
  }
  properties->resizeColumnsToContents();
  layout->addWidget(properties);
  return widget;
}

void ProcessEvents() {
  for (int i = 0; i < 3; ++i)
    QApplication::processEvents();
}

DockTabWidget& FindTabs(QMainWindow& main_window) {
  auto* tabs = main_window.findChild<DockTabWidget*>();
  EXPECT_NE(tabs, nullptr);
  return *tabs;
}

DockTabWidget& FindTabsContaining(QMainWindow& main_window, QWidget& widget) {
  for (auto* tabs : main_window.findChildren<DockTabWidget*>()) {
    if (tabs->indexOf(&widget) != -1)
      return *tabs;
  }
  ADD_FAILURE() << "Tab block not found";
  return FindTabs(main_window);
}

QImage RenderMainWindow(QMainWindow& main_window) {
  main_window.resize(520, 360);
  main_window.show();
  ProcessEvents();
  return main_window.grab().toImage();
}

int CompareImages(const QImage& actual, const QImage& expected) {
  if (actual.size() != expected.size())
    return -1;

  int diff_count = 0;
  for (int y = 0; y < actual.height(); ++y) {
    for (int x = 0; x < actual.width(); ++x) {
      if (actual.pixel(x, y) != expected.pixel(x, y))
        ++diff_count;
    }
  }
  return diff_count;
}

class ViewManagerQtComponentIntegrationTest : public testing::Test {
 protected:
  void ExpectMatchesGolden(QMainWindow& main_window, const QString& name) {
    QImage actual = RenderMainWindow(main_window);
    QDir testdata_dir{VIEW_MANAGER_QT_TESTDATA_DIR};
    testdata_dir.mkpath(".");

    const QString golden_path = testdata_dir.filePath(name);
    QImage expected{golden_path};
    if (expected.isNull()) {
      ASSERT_TRUE(actual.save(golden_path))
          << "Failed to save golden image: " << golden_path.toStdString();
      GTEST_SKIP() << "Golden image created. Re-run test to verify.";
    }

    const int diff_pixels = CompareImages(actual, expected);
    if (diff_pixels != 0) {
      const QString actual_path = testdata_dir.filePath("actual_" + name);
      actual.save(actual_path);
      FAIL() << "Rendering differs from golden image by " << diff_pixels
             << " pixels. Actual saved to: " << actual_path.toStdString();
    }
  }
};

TEST_F(ViewManagerQtComponentIntegrationTest, PublicApiWorkflow) {
  QMainWindow main_window;
  ViewManagerQtComponent component{main_window};

  auto primary = CreateEventList();
  auto secondary = CreateAssetTree();
  auto dock = CreatePropertiesView();

  std::vector<ViewManagerQtComponent::ViewId> closed_views;
  std::vector<std::optional<ViewManagerQtComponent::ViewId>> active_views;
  std::vector<ViewManagerQtComponent::ViewId> popup_views;
  std::vector<QPoint> popup_points;

  component.SetCloseViewHandler([&](ViewManagerQtComponent::ViewId view_id) {
    closed_views.emplace_back(view_id);
  });
  component.SetActiveViewChangedHandler(
      [&](std::optional<ViewManagerQtComponent::ViewId> view_id) {
        active_views.emplace_back(view_id);
      });
  component.SetTabPopupMenuHandler(
      [&](ViewManagerQtComponent::ViewId view_id, const QPoint& point) {
        popup_views.emplace_back(view_id);
        popup_points.emplace_back(point);
      });

  auto primary_view = MakeView(1, *primary, u"Primary");
  auto secondary_view = MakeView(2, *secondary, u"Secondary");
  auto dock_view = MakeView(3, *dock, u"Properties", /*dock=*/true,
                            /*dock_bottom=*/true);
  std::vector views{primary_view, secondary_view, dock_view};

  component.AddView(views[0], std::nullopt);
  component.AddView(views[1], views[0].id);
  component.AddView(views[2], std::nullopt);

  main_window.resize(520, 360);
  main_window.show();
  main_window.activateWindow();
  ProcessEvents();

  active_views.clear();
  component.ActivateView(views[1].id);
  ProcessEvents();

  EXPECT_EQ(component.GetActiveViewId(), views[1].id);
  ASSERT_FALSE(active_views.empty());
  EXPECT_EQ(active_views.back(), std::optional{views[1].id});

  component.SplitView(views[1].id, /*vertically=*/true);
  component.SetViewTitle(views[0].id, u"Primary Updated");
  component.SetViewTitle(views[2].id, u"Properties Updated");

  auto& tabs = FindTabsContaining(main_window, *primary);
  auto* tab_bar = tabs.tabBar();
  const QPoint tab_pos = tab_bar->tabRect(tabs.indexOf(primary.get())).center();
  ASSERT_TRUE(QMetaObject::invokeMethod(
      tab_bar, "customContextMenuRequested", Q_ARG(QPoint, tab_pos)));

  EXPECT_THAT(popup_views, ElementsAre(views[0].id));
  EXPECT_FALSE(popup_points.empty());

  auto layout = component.SaveLayout(views);
  EXPECT_EQ(layout.main.type,
            ViewManagerQtComponent::LayoutNode::Type::Split);
  EXPECT_FALSE(layout.main.split_vertical);
  EXPECT_FALSE(layout.dock_state_blob.empty());

  ExpectMatchesGolden(main_window, "component_workflow.png");

  ASSERT_TRUE(component.RemoveView(views[0].id));
  ASSERT_TRUE(component.RemoveView(views[1].id));
  ASSERT_TRUE(component.RemoveView(views[2].id));

  primary->setParent(nullptr);
  secondary->setParent(nullptr);
}

TEST_F(ViewManagerQtComponentIntegrationTest, OpenLayoutRestoresSavedShape) {
  QMainWindow main_window;
  ViewManagerQtComponent component{main_window};

  auto left = CreateAssetTree();
  auto top_right = CreateEventList();
  auto bottom_right = CreateTrendTable();
  auto bottom_dock = CreatePropertiesView();

  std::vector<ViewManagerQtComponent::ViewInfo> views{
      MakeView(1, *left, u"Left"),
      MakeView(2, *top_right, u"Top Right"),
      MakeView(3, *bottom_right, u"Bottom Right"),
      MakeView(4, *bottom_dock, u"Bottom Dock", /*dock=*/true,
               /*dock_bottom=*/true),
  };

  ViewManagerQtComponent::SavedLayout layout;
  layout.main.type = ViewManagerQtComponent::LayoutNode::Type::Split;
  layout.main.split_vertical = false;
  layout.main.split_pos = 40;
  layout.main.left = std::make_unique<ViewManagerQtComponent::LayoutNode>();
  layout.main.left->tabs = {views[0].id};
  layout.main.right = std::make_unique<ViewManagerQtComponent::LayoutNode>();
  layout.main.right->type = ViewManagerQtComponent::LayoutNode::Type::Split;
  layout.main.right->split_vertical = true;
  layout.main.right->split_pos = 55;
  layout.main.right->left =
      std::make_unique<ViewManagerQtComponent::LayoutNode>();
  layout.main.right->left->tabs = {views[1].id};
  layout.main.right->right =
      std::make_unique<ViewManagerQtComponent::LayoutNode>();
  layout.main.right->right->tabs = {views[2].id, views[3].id};

  component.OpenLayout(views, layout);

  auto saved_layout = component.SaveLayout(views);
  EXPECT_EQ(saved_layout.main.type,
            ViewManagerQtComponent::LayoutNode::Type::Split);
  ASSERT_TRUE(saved_layout.main.left);
  ASSERT_TRUE(saved_layout.main.right);
  EXPECT_THAT(saved_layout.main.left->tabs, ElementsAre(views[0].id));
  EXPECT_EQ(saved_layout.main.right->type,
            ViewManagerQtComponent::LayoutNode::Type::Split);
  ASSERT_TRUE(saved_layout.main.right->left);
  ASSERT_TRUE(saved_layout.main.right->right);
  EXPECT_THAT(saved_layout.main.right->left->tabs, ElementsAre(views[1].id));
  EXPECT_THAT(saved_layout.main.right->right->tabs, ElementsAre(views[2].id));

  auto docks = main_window.findChildren<QDockWidget*>();
  ASSERT_EQ(docks.size(), 1);
  EXPECT_EQ(docks[0]->windowTitle(), "Bottom Dock");

  ExpectMatchesGolden(main_window, "open_layout.png");

  ASSERT_TRUE(component.RemoveView(views[0].id));
  ASSERT_TRUE(component.RemoveView(views[1].id));
  ASSERT_TRUE(component.RemoveView(views[2].id));
  ASSERT_TRUE(component.RemoveView(views[3].id));

  left->setParent(nullptr);
  top_right->setParent(nullptr);
  bottom_right->setParent(nullptr);
}

TEST_F(ViewManagerQtComponentIntegrationTest, DockCloseUsesCloseHandler) {
  QMainWindow main_window;
  ViewManagerQtComponent component{main_window};

  auto* dock_widget = new QWidget;
  auto dock_view = MakeView(10, *dock_widget, u"Closable Dock", /*dock=*/true);

  std::vector<ViewManagerQtComponent::ViewId> closed_views;
  component.SetCloseViewHandler([&](ViewManagerQtComponent::ViewId view_id) {
    closed_views.emplace_back(view_id);
  });

  component.AddView(dock_view, std::nullopt);

  auto docks = main_window.findChildren<QDockWidget*>();
  ASSERT_EQ(docks.size(), 1);

  docks[0]->close();
  ProcessEvents();

  EXPECT_THAT(closed_views, ElementsAre(dock_view.id));
}

}  // namespace
