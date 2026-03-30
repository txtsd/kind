#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QWidget>

namespace kind {
class Client;
}

namespace kind::gui {

class LoginDialog : public QDialog {
  Q_OBJECT

public:
  explicit LoginDialog(QWidget* parent = nullptr);

  void show_error(const QString& message);
  void show_mfa_input();
  void enable_login();

  void load_saved_token(const std::string& token, const std::string& token_type);
  bool auto_login_enabled() const;

signals:
  void token_login_requested(QString token, QString token_type);
  void credential_login_requested(QString email, QString password);
  void mfa_code_submitted(QString code);

private:
  void setup_ui();
  void setup_connections();

  QTabWidget* tab_widget_{};

  // Token tab
  QLineEdit* token_input_{};
  QComboBox* token_type_combo_{};
  QPushButton* token_login_button_{};

  // Credentials tab
  QLineEdit* email_input_{};
  QLineEdit* password_input_{};
  QPushButton* credential_login_button_{};

  // MFA section
  QWidget* mfa_widget_{};
  QLineEdit* mfa_input_{};
  QPushButton* mfa_submit_button_{};

  // Auto-login
  QCheckBox* auto_login_checkbox_{};

  // Status
  QLabel* status_label_{};
};

} // namespace kind::gui
