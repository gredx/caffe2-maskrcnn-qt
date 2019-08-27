#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include <QMainWindow>
#include "classify.h"
#include "MaskRcnn.h"
namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();


private slots:


private:
    Ui::MainWindow *ui;
    Classify *c;
	MaskRcnn *m;
};

#endif // MAINWINDOW_H
