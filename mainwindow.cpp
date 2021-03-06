#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "console.h"
#include <QFileDialog>
#include <QDesktopServices>
#include <QDate>
#include <QRadioButton>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    cameraManager(new CameraManager(this)),
    fpsTimer(new QTimer(this))
{
    ui->setupUi(this);
    readSettings();
    showMaximized();
    Console::setOutputControl(ui->consoleOutput);

    QWidget *horizontalSpacer = new QWidget(ui->mainToolBar);
    horizontalSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    ui->mainToolBar->addWidget(horizontalSpacer);

    cameraComboBox = new DataAwareComboBox(ui->mainToolBar);
    cameraComboBoxAction = ui->mainToolBar->addWidget(cameraComboBox);

    connect(cameraComboBox, SIGNAL(activated(QVariant)), cameraManager, SLOT(changeSelectedCamera(QVariant)));
    connect(cameraManager, SIGNAL(changedSelectedCamera(QSharedPointer<QCamera>)), this, SLOT(onCameraChanged(QSharedPointer<QCamera>)));

    connect(fpsTimer, SIGNAL(timeout()), this, SLOT(updateFps()));
    connect(ui->cameraViewFinder->getVideoSurface(), SIGNAL(frameReceived(cv::Mat&)), this, SLOT(processFrame(cv::Mat&)));

    ui->consoleDockWidget->setVisible(false);
    ui->settingsDockWidget->setVisible(false);
    ui->filtersDockWidget->setVisible(false);

    detectCameras();
    ui->filtersFrame->setEnabled(false);
    registerFilters();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::detectCameras()
{
    QList<QCameraInfo> cameras = cameraManager->listCameras();

    if (cameras.empty()) {
        Console::error("No available cameras detected!");
        cameraComboBoxAction->setVisible(false);
        ui->consoleDockWidget->setVisible(true);
        return;
    }

    Console::log(QString("Detected %1 cameras:").arg(cameras.size()));

    foreach (const QCameraInfo &cameraInfo, cameras) {
        Console::log(cameraInfo.description());
        cameraComboBox->addItem(cameraInfo.description(), QVariant::fromValue(cameraInfo));
    }

    cameraManager->changeSelectedCamera(cameras.first());

    if (cameras.size() == 1) {
        cameraComboBoxAction->setDisabled(true);
    }
}

void MainWindow::onCameraChanged(const QSharedPointer<QCamera> &cameraPtr)
{
    ui->actionToggleCamera->setChecked(false);

    QCamera *camera = cameraPtr.data();
    camera->setViewfinder(ui->cameraViewFinder->getVideoSurface());
}

void MainWindow::toggleCamera(bool enable)
{
    QCamera *camera = cameraManager->getSelectedCamera().data();
    QCameraInfo cameraInfo = cameraManager->getSelectedCameraInfo();

    if (enable) {
        Console::log(QString("Starting camera %1").arg(cameraInfo.description()));
        camera->load();
        camera->start();
        fpsTimer->start(1000);
    } else {
        Console::log(QString("Stopping camera %1").arg(cameraInfo.description()));
        camera->stop();
        fpsTimer->stop();
        ui->statusBar->clearMessage();
    }
}

void MainWindow::processFrame(cv::Mat &mat)
{
    ++framesInCurrentSecond;
}

void MainWindow::updateFps()
{
    ui->statusBar->showMessage(QString("%1 FPS").arg(framesInCurrentSecond));
    framesInCurrentSecond = 0;
}

void MainWindow::chooseOutputDirectory()
{
    QString outDirPath = QFileDialog::getExistingDirectory(this, tr("Output directory"), outputDirectoryPath, QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (!outDirPath.isEmpty()) {
        outputDirectoryPath = outDirPath;
        ui->outDirEdit->setText(outputDirectoryPath);
        writeSettings();
        Console::log(QString("Changed output directory to %1").arg(outputDirectoryPath));
    }
}

void MainWindow::readSettings()
{
    QSettings settings;

    settings.beginGroup("output");
    outputDirectoryPath = settings.value("directory").toString();
    ui->outDirEdit->setText(outputDirectoryPath);
    settings.endGroup();
}

void MainWindow::writeSettings()
{
    QSettings settings;

    settings.beginGroup("output");
    settings.setValue("directory", outputDirectoryPath);
    settings.endGroup();
}

void MainWindow::openOutputDirectory()
{
    QString outputDirPath = QDir::toNativeSeparators(outputDirectoryPath);
    QDesktopServices::openUrl(QUrl::fromLocalFile(outputDirPath));
}

bool MainWindow::grabImage()
{
    QImage &renderedImage = ui->cameraViewFinder->getRenderedImage();

    QString fileName = QDateTime::currentDateTime().toString("''yyyyMMdd_HHmmsszzz'.jpg'");

    QString filePath = QDir(QDir::toNativeSeparators(outputDirectoryPath)).filePath(fileName);

    bool imageSaved = renderedImage.save(filePath, "JPG", 100);

    if (imageSaved) {
        Console::log(QString("Saved image to %1").arg(filePath));
    } else {
        Console::log(QString("Cold not save image to %1").arg(filePath));
    }

    return imageSaved;
}

void MainWindow::registerFilters()
{
    registerFilter(QSharedPointer<AbstractFilter>(new FaceDetectFilter()));
    registerFilter(QSharedPointer<AbstractFilter>(new GrayscaleFilter()));
}

void MainWindow::registerFilter(QSharedPointer<AbstractFilter> filterPtr)
{
    ui->cameraViewFinder->registerFilter(filterPtr);

    AbstractFilter *filter = filterPtr.data();

    QRadioButton *filterRadioButton = new QRadioButton(filter->getName(), ui->filtersFrame);
    ui->filtersFrame->layout()->addWidget(filterRadioButton);
    connect(filterRadioButton, SIGNAL(toggled(bool)), filter, SLOT(setEnabled(bool)));
}

void MainWindow::enableFilters(bool enabled)
{
    if (!enabled) {
        ui->cameraViewFinder->disableFilters();

        foreach(QObject *child, ui->filtersFrame->children()) {
            QRadioButton *filterRadioButton = qobject_cast<QRadioButton*>(child);

            if (filterRadioButton) {
                filterRadioButton->setAutoExclusive(false);
                filterRadioButton->setChecked(false);
                filterRadioButton->setAutoExclusive(true);
            }
        }
    }

    ui->filtersFrame->setEnabled(enabled);
}
