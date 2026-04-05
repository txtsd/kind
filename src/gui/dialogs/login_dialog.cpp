#include "login_dialog.hpp"

#include "logging.hpp"

#include <QFormLayout>
#include <QSettings>
#include <QVBoxLayout>

namespace kind::gui {

LoginDialog::LoginDialog(const std::vector<ConfigManager::KnownAccount>& known_accounts,
                         QWidget* parent)
    : QDialog(parent) {
  setWindowTitle("kind - Login");
  setMinimumWidth(400);
  setup_ui(known_accounts);
  setup_connections();
}

void LoginDialog::setup_ui(const std::vector<ConfigManager::KnownAccount>& known_accounts) {
  auto* main_layout = new QVBoxLayout(this);

  // Known accounts dropdown (shown above tabs when accounts exist)
  if (!known_accounts.empty()) {
    account_combo_ = new QComboBox(this);

    for (const auto& acct : known_accounts) {
      auto label = QString::fromStdString(acct.username)
                   + " (" + QString::number(acct.user_id) + ")";
      account_combo_->addItem(label, QVariant::fromValue(static_cast<qulonglong>(acct.user_id)));
    }
    account_combo_->addItem("New account", QVariant::fromValue(static_cast<qulonglong>(0)));

    auto* account_layout = new QFormLayout();
    account_layout->addRow("Account:", account_combo_);
    main_layout->addLayout(account_layout);

    log::gui()->debug("LoginDialog: populated {} known accounts in dropdown", known_accounts.size());
  }

  tab_widget_ = new QTabWidget(this);

  // Token tab
  auto* token_tab = new QWidget();
  auto* token_layout = new QFormLayout(token_tab);
  token_input_ = new QLineEdit(token_tab);
  token_input_->setPlaceholderText("Paste your token here");
  token_input_->setEchoMode(QLineEdit::Password);
  token_type_combo_ = new QComboBox(token_tab);
  token_type_combo_->addItem("User", "user");
  token_type_combo_->addItem("Bot (not yet supported)", "bot");
  // Disable bot token option until bot login is tested and fully implemented
  token_type_combo_->setItemData(1, false, Qt::UserRole - 1);
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
  tab_widget_->addTab(cred_tab, "Credentials (not yet supported)");
  // Disable credentials tab until email/password login is tested and fully implemented
  tab_widget_->setTabEnabled(1, false);

  main_layout->addWidget(tab_widget_);

  // Auto-login checkbox
  auto_login_checkbox_ = new QCheckBox("Auto-login with saved token", this);
  main_layout->addWidget(auto_login_checkbox_);

  // Persist checkbox state via QSettings
  QSettings settings("kind", "kind");
  auto_login_checkbox_->setChecked(settings.value("auto_login", false).toBool());
  connect(auto_login_checkbox_, &QCheckBox::toggled, [](bool checked) {
    QSettings s("kind", "kind");
    s.setValue("auto_login", checked);
  });

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

  // Wire account dropdown to load token from keychain
  if (account_combo_) {
    connect(account_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
      if (index < 0) return;
      auto user_id = account_combo_->itemData(index).value<qulonglong>();

      if (user_id == 0) {
        // "New account" selected: clear token input
        token_input_->clear();
        log::gui()->debug("LoginDialog: 'New account' selected, cleared token input");
        return;
      }

      log::gui()->debug("LoginDialog: account {} selected, loading token from keychain", user_id);

      if (token_loader_) {
        token_loader_(user_id, [this](const std::string& token, const std::string& token_type) {
          // This callback may be called asynchronously, so ensure we're on the UI thread
          QMetaObject::invokeMethod(this, [this, token, token_type]() {
            load_saved_token(token, token_type);
            log::gui()->debug("LoginDialog: token loaded for selected account");
          });
        });
      }
    });
  }
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

void LoginDialog::load_saved_token(const std::string& token, const std::string& token_type) {
  token_input_->setText(QString::fromStdString(token));

  for (int i = 0; i < token_type_combo_->count(); ++i) {
    if (token_type_combo_->itemData(i).toString().toStdString() == token_type) {
      token_type_combo_->setCurrentIndex(i);
      break;
    }
  }

  tab_widget_->setCurrentIndex(0);
}

bool LoginDialog::auto_login_enabled() const {
  return auto_login_checkbox_->isChecked();
}

void LoginDialog::set_token_loader(TokenLoader loader) {
  token_loader_ = std::move(loader);
}

bool LoginDialog::has_selected_account() const {
  if (!account_combo_ || account_combo_->currentIndex() < 0) return false;
  return account_combo_->currentData().value<qulonglong>() != 0;
}

void LoginDialog::load_selected_account_token() {
  if (!account_combo_ || !token_loader_) return;
  auto user_id = account_combo_->currentData().value<qulonglong>();
  if (user_id == 0) return;

  log::gui()->debug("LoginDialog: loading token for preselected account {}", user_id);
  token_loader_(user_id, [this](const std::string& token, const std::string& token_type) {
    QMetaObject::invokeMethod(this, [this, token, token_type]() {
      load_saved_token(token, token_type);
    });
  });
}

} // namespace kind::gui
