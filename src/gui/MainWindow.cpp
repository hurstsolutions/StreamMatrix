#include "MainWindow.h"
#include <QLabel>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("Event Streamer");
    resize(800, 600);
    QLabel *label = new QLabel("Hello, Event Streamer!", this);
    setCentralWidget(label);
}