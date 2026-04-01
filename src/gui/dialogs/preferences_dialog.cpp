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

  // -- Channel unread indicators --
  appearance_layout->addRow(new QLabel("<b>Channel unread indicators</b>"));

  channel_unread_dot_ = new QCheckBox("Show dot indicator");
  appearance_layout->addRow(channel_unread_dot_);

  channel_unread_badge_ = new QCheckBox("Show count badge");
  appearance_layout->addRow(channel_unread_badge_);

  channel_unread_glow_ = new QCheckBox("Show glow effect");
  appearance_layout->addRow(channel_unread_glow_);

  // -- Guild unread indicators --
  appearance_layout->addRow(new QLabel("<b>Guild unread indicators</b>"));

  guild_unread_dot_ = new QCheckBox("Show dot indicator");
  appearance_layout->addRow(guild_unread_dot_);

  guild_unread_badge_ = new QCheckBox("Show count badge");
  appearance_layout->addRow(guild_unread_badge_);

  guild_unread_glow_ = new QCheckBox("Show glow effect");
  appearance_layout->addRow(guild_unread_glow_);

  // -- Mention indicators --
  appearance_layout->addRow(new QLabel("<b>Mention indicators</b>"));

  mention_badge_channel_ = new QCheckBox("Badge on channels");
  appearance_layout->addRow(mention_badge_channel_);

  mention_badge_guild_ = new QCheckBox("Badge on guilds");
  appearance_layout->addRow(mention_badge_guild_);

  mention_highlight_channel_ = new QCheckBox("Highlight channels");
  appearance_layout->addRow(mention_highlight_channel_);

  mention_highlight_guild_ = new QCheckBox("Highlight guilds");
  appearance_layout->addRow(mention_highlight_guild_);

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

  // Channel unread indicators
  channel_unread_dot_->setChecked(config_.get_or<bool>("appearance.channel_unread_dot", true));
  channel_unread_badge_->setChecked(config_.get_or<bool>("appearance.channel_unread_badge", true));
  channel_unread_glow_->setChecked(config_.get_or<bool>("appearance.channel_unread_glow", false));

  // Guild unread indicators
  guild_unread_dot_->setChecked(config_.get_or<bool>("appearance.guild_unread_dot", true));
  guild_unread_badge_->setChecked(config_.get_or<bool>("appearance.guild_unread_badge", true));
  guild_unread_glow_->setChecked(config_.get_or<bool>("appearance.guild_unread_glow", false));

  // Mention indicators
  mention_badge_channel_->setChecked(config_.get_or<bool>("appearance.mention_badge_channel", true));
  mention_badge_guild_->setChecked(config_.get_or<bool>("appearance.mention_badge_guild", true));
  mention_highlight_channel_->setChecked(config_.get_or<bool>("appearance.mention_highlight_channel", false));
  mention_highlight_guild_->setChecked(config_.get_or<bool>("appearance.mention_highlight_guild", false));
}

void PreferencesDialog::save_settings() {
  config_.set<std::string>("appearance.edited_indicator",
                           edited_indicator_combo_->currentData().toString().toStdString());
  config_.set<std::string>("appearance.guild_display",
                           guild_display_combo_->currentData().toString().toStdString());
  config_.set<bool>("appearance.hide_locked_channels",
                    hide_locked_checkbox_->isChecked());

  // Channel unread indicators
  config_.set<bool>("appearance.channel_unread_dot", channel_unread_dot_->isChecked());
  config_.set<bool>("appearance.channel_unread_badge", channel_unread_badge_->isChecked());
  config_.set<bool>("appearance.channel_unread_glow", channel_unread_glow_->isChecked());

  // Guild unread indicators
  config_.set<bool>("appearance.guild_unread_dot", guild_unread_dot_->isChecked());
  config_.set<bool>("appearance.guild_unread_badge", guild_unread_badge_->isChecked());
  config_.set<bool>("appearance.guild_unread_glow", guild_unread_glow_->isChecked());

  // Mention indicators
  config_.set<bool>("appearance.mention_badge_channel", mention_badge_channel_->isChecked());
  config_.set<bool>("appearance.mention_badge_guild", mention_badge_guild_->isChecked());
  config_.set<bool>("appearance.mention_highlight_channel", mention_highlight_channel_->isChecked());
  config_.set<bool>("appearance.mention_highlight_guild", mention_highlight_guild_->isChecked());

  config_.save();
}

} // namespace kind::gui
