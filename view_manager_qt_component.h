#pragma once

#include "dock_tab_widget.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <QObject>
#include <QPoint>

class QMainWindow;
class QDockWidget;
class QWidget;

class ViewManagerQtComponent final : public QObject {
 public:
  using ViewId = std::uintptr_t;

  struct ViewInfo {
    ViewId id = 0;
    QWidget* widget = nullptr;
    std::u16string title;
    std::string state_name;
    bool dock = false;
    bool dock_bottom = false;
    bool tabify_existing_dock = true;
  };

  struct LayoutNode {
    enum class Type { Tabs, Split };

    LayoutNode();
    LayoutNode(LayoutNode&&) noexcept;
    LayoutNode& operator=(LayoutNode&&) noexcept;
    ~LayoutNode();

    Type type = Type::Tabs;
    std::vector<ViewId> tabs;
    bool split_vertical = false;
    int split_pos = -1;
    std::unique_ptr<LayoutNode> left;
    std::unique_ptr<LayoutNode> right;
  };

  struct SavedLayout {
    LayoutNode main;
    std::string dock_state_blob;
  };

  explicit ViewManagerQtComponent(QMainWindow& main_window);
  ~ViewManagerQtComponent() override;

  void SetCloseViewHandler(std::function<void(ViewId)> handler);
  void SetActiveViewChangedHandler(
      std::function<void(std::optional<ViewId>)> handler);
  void SetTabPopupMenuHandler(
      std::function<void(ViewId, const QPoint&)> handler);
  // Offers a "new view" button at the end of every tab strip, and calls
  // `handler` with the position to show a menu at when one is pressed.
  //
  // Unset, no button is drawn at all -- a `+` that does nothing is worse than
  // no `+`. It is read when a tab block is created, so set it before the first
  // OpenLayout/AddView, alongside the other handlers; blocks that already exist
  // are not revisited.
  //
  // The button belongs to the strip it sits in: pressing it makes that strip's
  // current view active before `handler` runs, so a host that opens the new
  // view beside the active one puts it in the strip the user actually clicked
  // rather than in whichever block happened to be active.
  void SetNewTabHandler(std::function<void(const QPoint&)> handler);

  void OpenLayout(std::span<const ViewInfo> views, const SavedLayout& layout);
  SavedLayout SaveLayout(std::span<const ViewInfo> views);

  void AddView(const ViewInfo& view, std::optional<ViewId> active_view_id);
  bool RemoveView(ViewId view_id);
  void ActivateView(ViewId view_id);
  void SplitView(ViewId view_id, bool vertically);
  void SetViewTitle(ViewId view_id, std::u16string_view title);

  std::optional<ViewId> GetActiveViewId() const;

 private:
  std::unique_ptr<DockTabWidget> CreateTabBlock();
  void DeleteTabBlock(DockTabWidget& tabs, bool later);
  DockTabWidget& SplitTabBlock(DockTabWidget& tabs,
                               DockTabWidget::DropSide side);

  std::unique_ptr<QWidget> OpenLayoutBlock(const LayoutNode& block);
  void SaveLayoutBlock(LayoutNode& block, QWidget& widget) const;

  void AddDockView(const ViewInfo& view);
  void AddTabView(const ViewInfo& view, std::optional<ViewId> active_view_id);

  const ViewInfo* FindViewInfo(ViewId view_id) const;
  std::optional<ViewId> FindViewIdByWidget(const QWidget* widget) const;
  bool IsViewAdded(ViewId view_id) const;

  QDockWidget* GetDockWidget(ViewId view_id) const;
  DockTabWidget* GetTabWidget(ViewId view_id) const;

  void OnFocusChanged(QObject* focus_object);
  void RegisterViews(std::span<const ViewInfo> views);
  void SetRootWidget(QWidget* widget);

  QMainWindow& main_window_;
  QWidget* root_widget_ = nullptr;

  std::vector<ViewInfo> views_;
  std::vector<ViewId> added_views_;
  std::vector<std::pair<ViewId, QDockWidget*>> dock_widgets_;

  std::function<void(ViewId)> close_view_handler_;
  std::function<void(std::optional<ViewId>)> active_view_changed_handler_;
  std::function<void(ViewId, const QPoint&)> tab_popup_menu_handler_;
  std::function<void(const QPoint&)> new_tab_handler_;
};
