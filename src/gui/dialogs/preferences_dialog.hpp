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
  QCheckBox* channel_unread_bar_{};
  QCheckBox* channel_unread_badge_{};

  // Guild unread indicators
  QCheckBox* guild_unread_bar_{};
  QCheckBox* guild_unread_badge_{};

  // Mention indicators
  QCheckBox* mention_badge_channel_{};
  QCheckBox* mention_badge_guild_{};

  // Mention colors
  QComboBox* mention_colors_combo_{};

  // Timestamps
  QCheckBox* show_timestamps_{};

  // Advanced page controls
  QComboBox* memory_profile_combo_{};
};

} // namespace kind::gui
