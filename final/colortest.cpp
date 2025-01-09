#include <iostream>
#include <windows.h>

void set_console_color(int color)
{
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, color);
}

int main()
{
    // 常用颜色代码
    const int BLACK = 0;
    const int BLUE = 1;
    const int GREEN = 2;
    const int CYAN = 3;
    const int RED = 4;
    const int MAGENTA = 5;
    const int BROWN = 6;
    const int LIGHTGRAY = 7;
    const int DARKGRAY = 8;
    const int LIGHTBLUE = 9;
    const int LIGHTGREEN = 10;
    const int LIGHTCYAN = 11;
    const int LIGHTRED = 12;
    const int LIGHTMAGENTA = 13;
    const int YELLOW = 14;
    const int WHITE = 15;

    // 展示颜色
    for (int color = 0; color <= 15; ++color)
    {
        set_console_color(color);
        std::cout << "This is color code " << color << std::endl;
    }

    // 恢复默认颜色
    set_console_color(WHITE);

    return 0;
}