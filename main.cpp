#include "game/App.hpp"

int main() {
    game::App app;
    app.init();
    app.run();
    app.shutdown();
    return 0;
}