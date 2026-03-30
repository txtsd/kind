#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

int main() {
  auto screen = ftxui::ScreenInteractive::Fullscreen();
  auto component = ftxui::Renderer([] { return ftxui::text("kind is not Discord") | ftxui::center; });
  screen.Loop(component);
  return 0;
}
