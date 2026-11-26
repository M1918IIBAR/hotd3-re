// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Entry point for hotd3-re

#include "core/game.h"

#include <iostream>
#include <exception>

#if defined(HOTD3_PLATFORM_WINDOWS) && defined(HOTD3_RELEASE)
#include <windows.h>

int WINAPI WinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/, LPSTR /*lpCmdLine*/, int /*nShowCmd*/) {
    int argc = __argc;
    char** argv = __argv;
#else
int main(int argc, char* argv[]) {
#endif
    try {
        hotd3::Game game;

        if (!game.init(argc, argv)) {
            std::cerr << "[Error] Failed to initialize hotd3-re engine.\n";
            return 1;
        }

        game.run();
        game.shutdown();

        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "[Fatal Error] Exception caught: " << ex.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "[Fatal Error] Unknown unhandled exception occurred.\n";
        return 1;
    }
}
