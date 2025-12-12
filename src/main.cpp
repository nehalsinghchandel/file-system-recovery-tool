#include <QApplication>
#include "MainWindow.h"
#include <iostream>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    app.setApplicationName("File System Recovery Tool");
    app.setOrganizationName("FileSystemTool");
    
    FileSystemTool::MainWindow window;
    window.resize(1400, 900);
    window.show();
    
    std::cout << "File System Recovery Tool" << std::endl;
    std::cout << "A user-space file system simulator with recovery and optimization" << std::endl;
    std::cout << "================================" << std::endl;
    
    return app.exec();
}
