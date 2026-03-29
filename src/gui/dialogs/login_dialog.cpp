#include "login_dialog.hpp"

#include <QFormLayout>
#include <QVBoxLayout>

namespace kind::gui {

LoginDialog::LoginDialog(QWidget* parent) : QDialog(parent) {
  setWindowTitle("kind - Login");
  setMinimumWidth(400);
  setup_ui();
  setup_connections();
}

void LoginDialog::setup_ui() {
  auto* main_layout = new QVBoxLayout(this);

  tab_widget_ = new QTabWidget(this);

  // Token tab
  auto* token_tab = new QWidget();
  auto* token_layout = new QFormLayout(token_tab);
  token_input_ = new QLineEdit(token_tab);
  token_input_->setPlaceholderText("Paste your token here");
  token_input_->setEchoMode(QLineEdit::Password);
  token_type_combo_ = new QComboBox(token_tab);
  token_type_combo_->addItem("User", "user");
  token_type_combo_->addItem("Bot", "bot");
  token_login_button_ = new QPushButton("Login", token_tab);
  token_layout->addRow("Token:", token_input_);
  token_layout->addRow("Type:", token_type_combo_);
  token_layout->addRow(token_login_button_);

  // Credentials tab
  auto* cred_tab = new QWidget();
  auto* cred_layout = new QFormLayout(cred_tab);
  email_input_ = new QLineEdit(cred_tab);
  email_input_->setPlaceholderText("Email address");
  password_input_ = new QLineEdit(cred_tab);
  password_input_->setPlaceholderText("Password");
  password_input_->setEchoMode(QLineEdit::Password);
  credential_login_button_ = new QPushButton("Login", cred_tab);
  cred_layout->addRow("Email:", email_input_);
  cred_layout->addRow("Password:", password_input_);
  cred_layout->addRow(credential_login_button_);

  tab_widget_->addTab(token_tab, "Token");
  tab_widget_->addTab(cred_tab, "Credentials");

  main_layout->addWidget(tab_widget_);

  // MFA section (hidden by default)
  mfa_widget_ = new QWidget(this);
  auto* mfa_layout = new QFormLayout(mfa_widget_);
  mfa_input_ = new QLineEdit(mfa_widget_);
  mfa_input_->setPlaceholderText("Enter MFA code");
  mfa_submit_button_ = new QPushButton("Submit", mfa_widget_);
  mfa_layout->addRow("MFA Code:", mfa_input_);
  mfa_layout->addRow(mfa_submit_button_);
  mfa_widget_->setVisible(false);
  main_layout->addWidget(mfa_widget_);

  // Status label
  status_label_ = new QLabel(this);
  status_label_->setStyleSheet("color: red;");
  status_label_->setWordWrap(true);
  status_label_->setVisible(false);
  main_layout->addWidget(status_label_);
}

void LoginDialog::setup_connections() {
  connect(token_login_button_, &QPushButton::clicked, this, [this]() {
    auto token = token_input_->text().trimmed();
    auto type = token_type_combo_->currentData().toString();
    if (!token.isEmpty()) {
      status_label_->setVisible(false);
      token_login_button_->setEnabled(false);
      credential_login_button_->setEnabled(false);
      status_label_->setStyleSheet("color: palette(text);");
      status_label_->setText("Logging in...");
      status_label_->setVisible(true);
      emit token_login_requested(token, type);
    }
  });

  connect(credential_login_button_, &QPushButton::clicked, this, [this]() {
    auto email = email_input_->text().trimmed();
    auto password = password_input_->text();
    if (!email.isEmpty() && !password.isEmpty()) {
      status_label_->setVisible(false);
      token_login_button_->setEnabled(false);
      credential_login_button_->setEnabled(false);
      status_label_->setStyleSheet("color: palette(text);");
      status_label_->setText("Logging in...");
      status_label_->setVisible(true);
      emit credential_login_requested(email, password);
    }
  });

  connect(mfa_submit_button_, &QPushButton::clicked, this, [this]() {
    auto code = mfa_input_->text().trimmed();
    if (!code.isEmpty()) {
      status_label_->setVisible(false);
      emit mfa_code_submitted(code);
    }
  });
}

void LoginDialog::show_error(const QString& message) {
  status_label_->setStyleSheet("color: red;");
  status_label_->setText(message);
  status_label_->setVisible(true);
  enable_login();
}

void LoginDialog::show_mfa_input() {
  mfa_widget_->setVisible(true);
  mfa_input_->setFocus();
  enable_login();
}

void LoginDialog::enable_login() {
  token_login_button_->setEnabled(true);
  credential_login_button_->setEnabled(true);
}

} // namespace kind::gui
