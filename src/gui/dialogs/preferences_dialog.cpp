#include "dialogs/preferences_dialog.hpp"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace kind::gui {

PreferencesDialog::PreferencesDialog(kind::ConfigManager& config, QWidget* parent)
    : QDialog(parent), config_(config) {
  setWindowTitle("Preferences");
  setMinimumSize(550, 400);
  setup_ui();
  load_settings();
}

void PreferencesDialog::setup_ui() {
  auto* main_layout = new QHBoxLayout();

  // Left: category sidebar
  category_list_ = new QListWidget();
  category_list_->setFixedWidth(150);
  category_list_->addItem("Appearance");
  category_list_->addItem("Network");
  category_list_->addItem("Privacy");
  category_list_->addItem("Advanced");
  category_list_->setCurrentRow(0);

  // Right: stacked pages
  pages_ = new QStackedWidget();

  // -- Appearance page --
  auto* appearance_page = new QWidget();
  auto* appearance_layout = new QFormLayout(appearance_page);

  edited_indicator_combo_ = new QComboBox();
  edited_indicator_combo_->addItem("Text", "text");
  edited_indicator_combo_->addItem("Pencil icon", "icon");
  edited_indicator_combo_->addItem("Both", "both");
  appearance_layout->addRow("Edited indicator:", edited_indicator_combo_);

  guild_display_combo_ = new QComboBox();
  guild_display_combo_->addItem("Text only", "text");
  guild_display_combo_->addItem("Icon and text", "icon_text");
  guild_display_combo_->addItem("Icon only", "icon");
  appearance_layout->addRow("Guild display:", guild_display_combo_);

  hide_locked_checkbox_ = new QCheckBox("Hide channels you cannot access");
  appearance_layout->addRow("Hide locked channels:", hide_locked_checkbox_);

  pages_->addWidget(appearance_page);

  // -- Network page (placeholder) --
  auto* network_page = new QWidget();
  auto* network_layout = new QVBoxLayout(network_page);
  network_layout->addWidget(new QLabel("No settings available yet"));
  network_layout->addStretch();
  pages_->addWidget(network_page);

  // -- Privacy page (placeholder) --
  auto* privacy_page = new QWidget();
  auto* privacy_layout = new QVBoxLayout(privacy_page);
  privacy_layout->addWidget(new QLabel("No settings available yet"));
  privacy_layout->addStretch();
  pages_->addWidget(privacy_page);

  // -- Advanced page (placeholder) --
  auto* advanced_page = new QWidget();
  auto* advanced_layout = new QVBoxLayout(advanced_page);
  advanced_layout->addWidget(new QLabel("No settings available yet"));
  advanced_layout->addStretch();
  pages_->addWidget(advanced_page);

  // Connect category selection to page switching
  connect(category_list_, &QListWidget::currentRowChanged,
          pages_, &QStackedWidget::setCurrentIndex);

  // Content area: pages + button box
  auto* right_layout = new QVBoxLayout();
  right_layout->addWidget(pages_, 1);

  button_box_ = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Apply | QDialogButtonBox::Cancel);
  right_layout->addWidget(button_box_);

  main_layout->addWidget(category_list_);
  main_layout->addLayout(right_layout, 1);
  setLayout(main_layout);

  // Button connections
  connect(button_box_, &QDialogButtonBox::accepted, this, [this]() {
    save_settings();
    emit settings_changed();
    accept();
  });

  connect(button_box_->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, [this]() {
    save_settings();
    emit settings_changed();
  });

  connect(button_box_, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void PreferencesDialog::load_settings() {
  // Edited indicator
  auto edited = config_.get_or<std::string>("appearance.edited_indicator", "text");
  int edited_idx = edited_indicator_combo_->findData(QString::fromStdString(edited));
  if (edited_idx >= 0) {
    edited_indicator_combo_->setCurrentIndex(edited_idx);
  }

  // Guild display
  auto guild_disp = config_.get_or<std::string>("appearance.guild_display", "text");
  int guild_idx = guild_display_combo_->findData(QString::fromStdString(guild_disp));
  if (guild_idx >= 0) {
    guild_display_combo_->setCurrentIndex(guild_idx);
  }

  // Hide locked channels
  hide_locked_checkbox_->setChecked(config_.get_or<bool>("appearance.hide_locked_channels", false));
}

void PreferencesDialog::save_settings() {
  config_.set<std::string>("appearance.edited_indicator",
                           edited_indicator_combo_->currentData().toString().toStdString());
  config_.set<std::string>("appearance.guild_display",
                           guild_display_combo_->currentData().toString().toStdString());
  config_.set<bool>("appearance.hide_locked_channels",
                    hide_locked_checkbox_->isChecked());
  config_.save();
}

} // namespace kind::gui
