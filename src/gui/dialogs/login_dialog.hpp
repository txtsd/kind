#pragma once

#include "config/config_manager.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QWidget>

#include <cstdint>
#include <functional>
#include <vector>

namespace kind {
class Client;
}

namespace kind::gui {

class LoginDialog : public QDialog {
  Q_OBJECT

public:
  // Callback type for loading a token from keychain for a given user_id
  using TokenLoader = std::function<void(uint64_t user_id, std::function<void(const std::string& token, const std::string& token_type)>)>;

  explicit LoginDialog(const std::vector<ConfigManager::KnownAccount>& known_accounts,
                       QWidget* parent = nullptr);

  void show_error(const QString& message);
  void show_mfa_input();
  void enable_login();

  void load_saved_token(const std::string& token, const std::string& token_type);
  bool auto_login_enabled() const;

  // Set the callback used to load tokens for known accounts
  void set_token_loader(TokenLoader loader);

  // Whether a known account (not "New account") is selected in the dropdown
  bool has_selected_account() const;

  // Load the token for whatever account is currently selected in the dropdown
  void load_selected_account_token();

signals:
  void token_login_requested(QString token, QString token_type);
  void credential_login_requested(QString email, QString password);
  void mfa_code_submitted(QString code);

private:
  void setup_ui(const std::vector<ConfigManager::KnownAccount>& known_accounts);
  void setup_connections();

  QTabWidget* tab_widget_{};

  // Known accounts dropdown
  QComboBox* account_combo_{};

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

  // Token loader callback
  TokenLoader token_loader_;
};

} // namespace kind::gui
