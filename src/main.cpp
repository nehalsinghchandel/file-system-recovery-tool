#include "MainWindow.h"
#include <QApplication>
#include <iostream>

using namespace FileSystemTool;

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    std::cout << "File System Recovery Tool" << std::endl;
    std::cout << "A user-space file system simulator with recovery and optimization" << std::endl;
    std::cout << "================================" << std::endl;
    
    MainWindow window;
    window.show();
    
    return app.exec();
}
