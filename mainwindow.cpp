#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "graphwidget.h"

#include <QFileDialog>
#include <QVBoxLayout>
#include <QToolBar>
#include <QPushButton>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , graphWidget(new GraphWidget(this))
{
    ui->setupUi(this);
    setWindowTitle("Алгоритм Дейкстры");
    resize(800, 600);

    setCentralWidget(graphWidget);

    QToolBar *toolbar = addToolBar("Управление");
    toolbar->setMovable(false);
    carBtn = new QPushButton("▶ Запустить машину");
    carBtn->setEnabled(false);
    toolbar->addWidget(carBtn);

    connect(carBtn, &QPushButton::clicked, this, [this]() {
        if (graphWidget->isAnimRunning()) {
            graphWidget->stopCarAnimation();
            carBtn->setText("▶ Запустить машину");
        } else {
            graphWidget->startCarAnimation();
            carBtn->setText("⏹ Стоп");
        }
    });

    connect(graphWidget, &GraphWidget::pathFound, this, [this]() {
        carBtn->setEnabled(true);
        if (graphWidget->isAnimRunning()) {
            graphWidget->stopCarAnimation();
        }
        carBtn->setText("▶ Запустить машину");
    });

    connect(graphWidget, &GraphWidget::animationFinished, this, [this]() {
        carBtn->setText("▶ Запустить машину");
    });

    connect(graphWidget, &GraphWidget::statusMessage, this, [this](const QString &msg) {
        statusBar()->showMessage(msg);
    });

    QString filePath = QFileDialog::getOpenFileName(
        this, "Открыть файл графа", "", "Текстовые файлы (*.txt);;Все файлы (*)");

    if (!filePath.isEmpty()) {
        graphWidget->loadFromFile(filePath);
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}
