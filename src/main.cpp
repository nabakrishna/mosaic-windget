#include <windows.h>
#include "app/Application.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    mosaic::app::Application app;
    return app.Run(hInstance, nCmdShow);
}
