#include "view_manager_qt_component.h"

#include "dock_tab_widget.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDockWidget>
#include <QHBoxLayout>
#include <QMainWindow>
#include <QSplitter>

#include <algorithm>
#include <cassert>
#include <limits>
#include <ranges>

namespace {

class CustomDockWidget : public QDockWidget {
 public:
  explicit CustomDockWidget(QWidget* parent = nullptr) : QDockWidget(parent) {}

  using ClosedHandler = std::function<void()>;
  void SetClosedHandler(ClosedHandler handler) {
    closed_handler_ = std::move(handler);
  }

 protected:
  void closeEvent(QCloseEvent* e) override {
    if (closed_handler_)
      closed_handler_();
    e->accept();
  }

 private:
  ClosedHandler closed_handler_;
};

const char sc_layoutWidgetTypeProp[] = "LayoutWidgetType";

enum class LayoutWidgetType { Splitter, Tabs };

DockTabWidget* GetTabWidget(QWidget* widget) {
  if (!widget)
    return nullptr;

  QWidget* parent = widget->parentWidget();
  if (!parent)
    return nullptr;

  return qobject_cast<DockTabWidget*>(parent->parentWidget());
}

DockTabWidget* GetFirstTabBlock(QWidget& widget) {
  auto type_prop = widget.property(sc_layoutWidgetTypeProp);
  assert(!type_prop.isNull());

  auto type = static_cast<LayoutWidgetType>(type_prop.toInt());
  if (type == LayoutWidgetType::Splitter) {
    auto& splitter = static_cast<QSplitter&>(widget);
    if (auto* left = splitter.widget(0))
      return GetFirstTabBlock(*left);
    if (auto* right = splitter.widget(1))
      return GetFirstTabBlock(*right);
    return nullptr;
  }

  if (type == LayoutWidgetType::Tabs)
    return &static_cast<DockTabWidget&>(widget);

  return nullptr;
}

}  // namespace

ViewManagerQtComponent::LayoutNode::LayoutNode() = default;
ViewManagerQtComponent::LayoutNode::LayoutNode(LayoutNode&&) noexcept = default;
ViewManagerQtComponent::LayoutNode&
ViewManagerQtComponent::LayoutNode::operator=(LayoutNode&&) noexcept = default;
ViewManagerQtComponent::LayoutNode::~LayoutNode() = default;

ViewManagerQtComponent::ViewManagerQtComponent(QMainWindow& main_window)
    : main_window_{main_window} {
  auto* central_widget = new QWidget;
  central_widget->setLayout(new QHBoxLayout);
  central_widget->layout()->setContentsMargins(0, 0, 0, 0);
  main_window_.setCentralWidget(central_widget);

  QObject::connect(static_cast<QGuiApplication*>(QApplication::instance()),
                   &QGuiApplication::focusObjectChanged, this,
                   &ViewManagerQtComponent::OnFocusChanged);
}

ViewManagerQtComponent::~ViewManagerQtComponent() = default;

void ViewManagerQtComponent::SetCloseViewHandler(
    std::function<void(ViewId)> handler) {
  close_view_handler_ = std::move(handler);
}

void ViewManagerQtComponent::SetActiveViewChangedHandler(
    std::function<void(std::optional<ViewId>)> handler) {
  active_view_changed_handler_ = std::move(handler);
}

void ViewManagerQtComponent::SetTabPopupMenuHandler(
    std::function<void(ViewId, const QPoint&)> handler) {
  tab_popup_menu_handler_ = std::move(handler);
}

void ViewManagerQtComponent::OpenLayout(std::span<const ViewInfo> views,
                                        const SavedLayout& layout) {
  RegisterViews(views);
  added_views_.clear();
  dock_widgets_.clear();

  SetRootWidget(OpenLayoutBlock(layout.main).release());

  for (const auto& view : views_) {
    if (!IsViewAdded(view.id))
      AddView(view, std::nullopt);
  }

  if (!layout.dock_state_blob.empty()) {
    for (auto& [view_id, dock] : dock_widgets_) {
      const auto* view = FindViewInfo(view_id);
      dock->setObjectName(view ? QString::fromStdString(view->state_name) : "");
    }

    main_window_.restoreState(QByteArray::fromRawData(
        layout.dock_state_blob.data(), layout.dock_state_blob.size()));

    for (auto& [view_id, dock] : dock_widgets_) {
      dock->setObjectName({});
    }
  }
}

ViewManagerQtComponent::SavedLayout ViewManagerQtComponent::SaveLayout(
    std::span<const ViewInfo> views) {
  RegisterViews(views);

  SavedLayout layout;
  if (root_widget_)
    SaveLayoutBlock(layout.main, *root_widget_);

  for (auto& [view_id, dock] : dock_widgets_) {
    const auto* view = FindViewInfo(view_id);
    dock->setObjectName(view ? QString::fromStdString(view->state_name) : "");
  }

  auto blob = main_window_.saveState().toStdString();
  layout.dock_state_blob.assign(blob.begin(), blob.end());

  for (auto& [view_id, dock] : dock_widgets_) {
    dock->setObjectName({});
  }

  return layout;
}

void ViewManagerQtComponent::AddView(const ViewInfo& view,
                                     std::optional<ViewId> active_view_id) {
  if (auto i = std::ranges::find(views_, view.id, &ViewInfo::id);
      i != views_.end()) {
    *i = view;
  } else {
    views_.emplace_back(view);
  }

  if (view.dock)
    AddDockView(view);
  else
    AddTabView(view, active_view_id);
}

bool ViewManagerQtComponent::RemoveView(ViewId view_id) {
  const auto* view = FindViewInfo(view_id);
  if (!view || !view->widget)
    return false;

  if (auto* tabs = GetTabWidget(view_id)) {
    auto index = tabs->indexOf(view->widget);
    assert(index != -1);
    tabs->removeTab(index);

    // The view lifetime is owned by the caller, not by the tab widget.
    view->widget->setParent(nullptr);

    if (tabs->count() == 0 && tabs != root_widget_)
      DeleteTabBlock(*tabs, false);

  } else if (auto* dock = GetDockWidget(view_id)) {
    dock->deleteLater();
  } else {
    return false;
  }

  std::erase(added_views_, view_id);
  std::erase_if(dock_widgets_,
                [view_id](const auto& item) { return item.first == view_id; });
  std::erase_if(views_,
                [view_id](const ViewInfo& view) { return view.id == view_id; });
  return true;
}

void ViewManagerQtComponent::ActivateView(ViewId view_id) {
  const auto* view = FindViewInfo(view_id);
  if (!view || !view->widget)
    return;

  if (auto* tabs = GetTabWidget(view_id)) {
    auto index = tabs->indexOf(view->widget);
    assert(index != -1);
    tabs->setCurrentIndex(index);
  } else if (auto* dock = GetDockWidget(view_id)) {
    // A docked view has no DockTabWidget above it — its parent chain is
    // QDockWidget -> QMainWindow — so GetTabWidget returns null and the branch
    // above cannot front it. When docks are tabified together, raise() is what
    // selects the dock's tab; without it activating a pane only moved focus and
    // left the wrong pane on top.
    dock->show();
    dock->raise();
  }

  view->widget->setFocus(Qt::ActiveWindowFocusReason);
}

void ViewManagerQtComponent::SplitView(ViewId view_id, bool vertically) {
  const auto* view = FindViewInfo(view_id);
  if (!view || view->dock || !view->widget)
    return;

  auto* tabs = GetTabWidget(view_id);
  if (!tabs || tabs->count() < 2)
    return;

  auto tab_index = tabs->indexOf(view->widget);
  assert(tab_index != -1);
  tabs->removeTab(tab_index);

  auto side = vertically ? DockTabWidget::DropSide::Right
                         : DockTabWidget::DropSide::Bottom;
  auto& new_tabs = SplitTabBlock(*tabs, side);
  new_tabs.addTab(view->widget, QString::fromStdU16String(view->title));
}

void ViewManagerQtComponent::SetViewTitle(ViewId view_id,
                                          std::u16string_view title) {
  auto view = std::ranges::find(views_, view_id, &ViewInfo::id);
  if (view == views_.end())
    return;

  view->title = std::u16string{title};

  if (auto* tabs = GetTabWidget(view_id)) {
    if (auto index = tabs->indexOf(view->widget); index != -1)
      tabs->setTabText(index, QString::fromStdU16String(view->title));

  } else if (auto* dock = GetDockWidget(view_id)) {
    dock->setWindowTitle(QString::fromStdU16String(view->title));
  }
}

std::optional<ViewManagerQtComponent::ViewId>
ViewManagerQtComponent::GetActiveViewId() const {
  return FindViewIdByWidget(
      qobject_cast<QWidget*>(QApplication::focusObject()));
}

std::unique_ptr<DockTabWidget> ViewManagerQtComponent::CreateTabBlock() {
  auto tabs = std::make_unique<DockTabWidget>();
  tabs->setDocumentMode(true);
  tabs->setMovable(true);
  tabs->setTabsClosable(true);
  tabs->setProperty(sc_layoutWidgetTypeProp,
                    static_cast<int>(LayoutWidgetType::Tabs));

  auto* tabs_ptr = tabs.get();

  QObject::connect(tabs_ptr, &DockTabWidget::tabCloseRequested, this,
                   [this, tabs_ptr](int index) {
                     auto view_id = FindViewIdByWidget(tabs_ptr->widget(index));
                     if (view_id && close_view_handler_)
                       close_view_handler_(*view_id);
                   });

  QObject::connect(
      tabs_ptr, &DockTabWidget::tabDropped, this,
      [this, tabs_ptr](DockTabWidget& source, int source_index,
                       DockTabWidget::DropSide side) {
        if (&source == tabs_ptr && side == DockTabWidget::DropSide::Center)
          return;

        auto* source_widget = source.widget(source_index);
        assert(source_widget);
        auto source_text = source.tabText(source_index);
        source.removeTab(source_index);

        auto* target = tabs_ptr;
        if (side != DockTabWidget::DropSide::Center)
          target = &SplitTabBlock(*tabs_ptr, side);

        int index = target->addTab(source_widget, source_text);
        target->setCurrentIndex(index);

        if (source.count() == 0)
          DeleteTabBlock(source, true);
      });

  auto tab_bar = tabs_ptr->tabBar();
  tab_bar->setContextMenuPolicy(Qt::CustomContextMenu);
  QObject::connect(tab_bar, &DockTabWidget::customContextMenuRequested, this,
                   [this, tabs_ptr, tab_bar](const QPoint& pos) {
                     int index = tab_bar->tabAt(pos);
                     if (index == -1)
                       return;
                     auto view_id = FindViewIdByWidget(tabs_ptr->widget(index));
                     if (!view_id || !tab_popup_menu_handler_)
                       return;
                     tab_popup_menu_handler_(*view_id,
                                             tab_bar->mapToGlobal(pos));
                   });

  return tabs;
}

void ViewManagerQtComponent::DeleteTabBlock(DockTabWidget& tabs, bool later) {
  assert(tabs.count() == 0);
  assert(&tabs != root_widget_);

  auto* splitter = qobject_cast<QSplitter*>(tabs.parentWidget());
  assert(splitter);

  int index = splitter->indexOf(&tabs);
  assert(index != -1);

  int other_index = (index + 1) % 2;
  auto* other_widget = splitter->widget(other_index);
  assert(other_widget);

  if (later) {
    tabs.setParent(nullptr);
    tabs.deleteLater();
  }

  if (splitter == root_widget_) {
    SetRootWidget(other_widget);

  } else {
    auto* parent_splitter = qobject_cast<QSplitter*>(splitter->parentWidget());
    assert(parent_splitter);

    int splitter_index = parent_splitter->indexOf(splitter);
    assert(splitter_index != -1);

    parent_splitter->replaceWidget(splitter_index, other_widget);
    delete splitter;
  }
}

DockTabWidget& ViewManagerQtComponent::SplitTabBlock(
    DockTabWidget& tabs,
    DockTabWidget::DropSide side) {
  assert(side != DockTabWidget::DropSide::Center);

  auto* splitter = new QSplitter;
  splitter->setProperty(sc_layoutWidgetTypeProp,
                        static_cast<int>(LayoutWidgetType::Splitter));
  splitter->setOrientation((side == DockTabWidget::DropSide::Top ||
                            side == DockTabWidget::DropSide::Bottom)
                               ? Qt::Vertical
                               : Qt::Horizontal);
  splitter->setChildrenCollapsible(false);

  if (&tabs == root_widget_) {
    tabs.setParent(nullptr);
    root_widget_ = nullptr;
    SetRootWidget(splitter);

  } else {
    auto* parent_splitter = qobject_cast<QSplitter*>(tabs.parentWidget());
    assert(parent_splitter);
    int parent_index = parent_splitter->indexOf(&tabs);
    assert(parent_index != -1);
    parent_splitter->replaceWidget(parent_index, splitter);
  }

  auto* new_tabs = CreateTabBlock().release();

  if (side == DockTabWidget::DropSide::Right ||
      side == DockTabWidget::DropSide::Bottom) {
    splitter->addWidget(&tabs);
    splitter->addWidget(new_tabs);
  } else {
    splitter->addWidget(new_tabs);
    splitter->addWidget(&tabs);
  }

  return *new_tabs;
}

std::unique_ptr<QWidget> ViewManagerQtComponent::OpenLayoutBlock(
    const LayoutNode& block) {
  if (block.type == LayoutNode::Type::Tabs) {
    std::vector<const ViewInfo*> tab_views;
    for (ViewId view_id : block.tabs) {
      auto* view = FindViewInfo(view_id);
      if (!view)
        continue;

      if (view->dock)
        AddDockView(*view);
      else
        tab_views.emplace_back(view);
    }

    if (tab_views.empty())
      return nullptr;

    auto tabs = CreateTabBlock();
    for (const auto* view : tab_views) {
      if (!IsViewAdded(view->id)) {
        tabs->addTab(view->widget, QString::fromStdU16String(view->title));
        added_views_.emplace_back(view->id);
      }
    }
    return tabs;
  }

  auto widget1 = block.left ? OpenLayoutBlock(*block.left) : nullptr;
  auto widget2 = block.right ? OpenLayoutBlock(*block.right) : nullptr;
  if (!widget1)
    return widget2;
  if (!widget2)
    return widget1;

  auto splitter = std::make_unique<QSplitter>();
  splitter->setOrientation(block.split_vertical ? Qt::Vertical
                                                : Qt::Horizontal);
  splitter->setProperty(sc_layoutWidgetTypeProp,
                        static_cast<int>(LayoutWidgetType::Splitter));
  splitter->addWidget(widget1.release());
  splitter->addWidget(widget2.release());
  if (block.split_pos >= 0) {
    splitter->setSizes(
        {std::numeric_limits<int>::max() / 100 * block.split_pos,
         std::numeric_limits<int>::max() / 100 * (100 - block.split_pos)});
  }
  return splitter;
}

void ViewManagerQtComponent::SaveLayoutBlock(LayoutNode& block,
                                             QWidget& widget) const {
  auto type_prop = widget.property(sc_layoutWidgetTypeProp);
  assert(!type_prop.isNull());

  auto type = static_cast<LayoutWidgetType>(type_prop.toInt());
  if (type == LayoutWidgetType::Splitter) {
    auto& splitter = static_cast<QSplitter&>(widget);
    block.type = LayoutNode::Type::Split;
    block.split_vertical = splitter.orientation() == Qt::Vertical;
    auto sizes = splitter.sizes();
    assert(sizes.size() == 2);
    int total_size = sizes[0] + sizes[1];
    block.split_pos = total_size != 0 ? sizes[0] * 100 / total_size : 50;
    block.left = std::make_unique<LayoutNode>();
    block.right = std::make_unique<LayoutNode>();
    if (auto* left = splitter.widget(0))
      SaveLayoutBlock(*block.left, *left);
    if (auto* right = splitter.widget(1))
      SaveLayoutBlock(*block.right, *right);

  } else if (type == LayoutWidgetType::Tabs) {
    auto& tabs = static_cast<DockTabWidget&>(widget);
    block.type = LayoutNode::Type::Tabs;
    block.tabs.clear();
    for (int i = 0; i < tabs.count(); ++i) {
      auto view_id = FindViewIdByWidget(tabs.widget(i));
      if (view_id)
        block.tabs.emplace_back(*view_id);
    }
  }
}

void ViewManagerQtComponent::AddDockView(const ViewInfo& view) {
  assert(view.widget);
  assert(!IsViewAdded(view.id));

  auto area =
      view.dock_bottom ? Qt::BottomDockWidgetArea : Qt::LeftDockWidgetArea;
  auto* dock = new CustomDockWidget(&main_window_);
  dock->setWindowTitle(QString::fromStdU16String(view.title));
  dock->setWidget(view.widget);
  dock->SetClosedHandler([this, view_id = view.id] {
    if (close_view_handler_)
      close_view_handler_(view_id);
  });

  QDockWidget* tabify_to = nullptr;
  if (view.tabify_existing_dock) {
    for (auto* dock_widget : main_window_.findChildren<QDockWidget*>()) {
      auto* opened_dock = dynamic_cast<CustomDockWidget*>(dock_widget);
      if (!opened_dock)
        continue;
      if (main_window_.dockWidgetArea(opened_dock) == area) {
        tabify_to = opened_dock;
        break;
      }
    }
  }

  if (tabify_to)
    main_window_.tabifyDockWidget(tabify_to, dock);
  else
    main_window_.addDockWidget(area, dock);

  dock_widgets_.emplace_back(view.id, dock);
  added_views_.emplace_back(view.id);
}

void ViewManagerQtComponent::AddTabView(const ViewInfo& view,
                                        std::optional<ViewId> active_view_id) {
  assert(view.widget);
  assert(!IsViewAdded(view.id));

  auto* tabs = active_view_id ? GetTabWidget(*active_view_id) : nullptr;
  if (!tabs && root_widget_)
    tabs = GetFirstTabBlock(*root_widget_);

  if (!tabs) {
    tabs = CreateTabBlock().release();
    SetRootWidget(tabs);
  }

  tabs->addTab(view.widget, QString::fromStdU16String(view.title));
  added_views_.emplace_back(view.id);
}

const ViewManagerQtComponent::ViewInfo* ViewManagerQtComponent::FindViewInfo(
    ViewId view_id) const {
  auto i = std::ranges::find(views_, view_id, &ViewInfo::id);
  return i != views_.end() ? &*i : nullptr;
}

std::optional<ViewManagerQtComponent::ViewId>
ViewManagerQtComponent::FindViewIdByWidget(const QWidget* widget) const {
  if (!widget)
    return std::nullopt;

  auto i = std::ranges::find_if(views_, [widget](const ViewInfo& view) {
    return view.widget &&
           (view.widget == widget || view.widget->isAncestorOf(widget));
  });
  if (i == views_.end())
    return std::nullopt;
  return i->id;
}

bool ViewManagerQtComponent::IsViewAdded(ViewId view_id) const {
  return std::ranges::find(added_views_, view_id) != added_views_.end();
}

QDockWidget* ViewManagerQtComponent::GetDockWidget(ViewId view_id) const {
  auto i = std::ranges::find(dock_widgets_, view_id,
                             [](const auto& item) { return item.first; });
  return i != dock_widgets_.end() ? i->second : nullptr;
}

DockTabWidget* ViewManagerQtComponent::GetTabWidget(ViewId view_id) const {
  const auto* view = FindViewInfo(view_id);
  return view ? ::GetTabWidget(view->widget) : nullptr;
}

void ViewManagerQtComponent::OnFocusChanged(QObject* focus_object) {
  if (!active_view_changed_handler_)
    return;

  auto view_id = FindViewIdByWidget(qobject_cast<QWidget*>(focus_object));
  if (view_id)
    active_view_changed_handler_(view_id);
}

void ViewManagerQtComponent::RegisterViews(std::span<const ViewInfo> views) {
  views_.assign(views.begin(), views.end());
}

void ViewManagerQtComponent::SetRootWidget(QWidget* widget) {
  if (root_widget_ == widget)
    return;

  if (root_widget_) {
    if (widget)
      widget->setParent(nullptr);

    delete root_widget_;
    root_widget_ = nullptr;
  }

  root_widget_ = widget;

  if (root_widget_)
    main_window_.centralWidget()->layout()->addWidget(root_widget_);
}
