#include "dialogs/new_dm_dialog.hpp"

#include <QDialogButtonBox>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>

namespace kind::gui {

NewDmDialog::NewDmDialog(const std::vector<kind::User>& users, QWidget* parent)
    : QDialog(parent), all_users_(users) {
  setWindowTitle("New Direct Message");
  setMinimumSize(300, 400);

  auto* layout = new QVBoxLayout(this);

  search_input_ = new QLineEdit(this);
  search_input_->setPlaceholderText("Search users...");
  layout->addWidget(search_input_);

  user_list_ = new QListWidget(this);
  layout->addWidget(user_list_);

  auto* buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
  layout->addWidget(buttons);

  // Sort users alphabetically
  std::sort(all_users_.begin(), all_users_.end(),
            [](const kind::User& a, const kind::User& b) {
              return a.username < b.username;
            });

  // Populate initially
  filter_users("");

  connect(search_input_, &QLineEdit::textChanged,
          this, &NewDmDialog::filter_users);

  connect(user_list_, &QListWidget::currentItemChanged, this,
          [buttons](QListWidgetItem* current, QListWidgetItem*) {
            buttons->button(QDialogButtonBox::Ok)->setEnabled(current != nullptr);
          });

  connect(user_list_, &QListWidget::itemDoubleClicked, this, [this]() {
    on_item_selected();
    accept();
  });

  connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
    on_item_selected();
    accept();
  });

  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void NewDmDialog::filter_users(const QString& text) {
  user_list_->clear();
  auto filter = text.toLower();
  for (const auto& user : all_users_) {
    auto name = QString::fromStdString(user.username);
    if (filter.isEmpty() || name.toLower().contains(filter)) {
      auto* item = new QListWidgetItem(name, user_list_);
      item->setData(Qt::UserRole,
                    QVariant::fromValue(static_cast<qulonglong>(user.id)));
    }
  }
}

void NewDmDialog::on_item_selected() {
  auto* item = user_list_->currentItem();
  if (item) {
    selected_id_ = item->data(Qt::UserRole).value<qulonglong>();
  }
}

} // namespace kind::gui
