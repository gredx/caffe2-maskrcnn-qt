#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
     c = new Classify(this);
	 m = new MaskRcnn(this);
     ui->tabWidget->clear();
     ui->tabWidget->addTab(c,"Classify");
	 ui->tabWidget->addTab(m, "Mask-Rcnn");
}

MainWindow::~MainWindow()
{
    delete c;
	delete m;
    delete ui;

}


