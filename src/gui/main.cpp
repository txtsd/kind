#include <QApplication>

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  app.setApplicationName("kind");
  app.setApplicationVersion("0.1.0");
  return app.exec();
}
