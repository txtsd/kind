#pragma once

#include "models/user.hpp"
#include "models/snowflake.hpp"

#include <QDialog>
#include <vector>

class QLineEdit;
class QListWidget;
class QListWidgetItem;

namespace kind::gui {

class NewDmDialog : public QDialog {
  Q_OBJECT

public:
  explicit NewDmDialog(const std::vector<kind::User>& users, QWidget* parent = nullptr);

  kind::Snowflake selected_user_id() const { return selected_id_; }

private:
  void filter_users(const QString& text);
  void on_item_selected();

  QLineEdit* search_input_;
  QListWidget* user_list_;
  std::vector<kind::User> all_users_;
  kind::Snowflake selected_id_{0};
};

} // namespace kind::gui
