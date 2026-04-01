#pragma once

#include "config/config_manager.hpp"

#include <QComboBox>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QListWidget>
#include <QStackedWidget>

namespace kind::gui {

class PreferencesDialog : public QDialog {
  Q_OBJECT

public:
  explicit PreferencesDialog(kind::ConfigManager& config, QWidget* parent = nullptr);

signals:
  void settings_changed();

private:
  void setup_ui();
  void load_settings();
  void save_settings();

  kind::ConfigManager& config_;

  QListWidget* category_list_{};
  QStackedWidget* pages_{};
  QDialogButtonBox* button_box_{};

  // Appearance page controls
  QComboBox* edited_indicator_combo_{};
  QComboBox* guild_display_combo_{};
  QCheckBox* hide_locked_checkbox_{};

  // Channel unread indicators
  QCheckBox* channel_unread_dot_{};
  QCheckBox* channel_unread_badge_{};
  QCheckBox* channel_unread_glow_{};

  // Guild unread indicators
  QCheckBox* guild_unread_dot_{};
  QCheckBox* guild_unread_badge_{};
  QCheckBox* guild_unread_glow_{};

  // Mention indicators
  QCheckBox* mention_badge_channel_{};
  QCheckBox* mention_badge_guild_{};
  QCheckBox* mention_highlight_channel_{};
  QCheckBox* mention_highlight_guild_{};
};

} // namespace kind::gui
