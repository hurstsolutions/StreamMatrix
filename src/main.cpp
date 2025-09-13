#include <QApplication>
#include "gui/MainWindow.h"
#include <gst/gst.h>

int main(int argc, char* argv[]) {
  // Initialize GStreamer before Qt uses any of it
  gst_init(&argc, &argv);

  QApplication app(argc, argv);
  MainWindow w;
  w.show();
  return app.exec();
}