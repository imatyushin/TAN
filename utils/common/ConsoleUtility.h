#pragma once
#include <QCommandLineParser>
#include <iostream>
#include <QtWidgets/QApplication>
#include "QTRoomAcousticConfig.h"

bool initFromCommandLine(QApplication& application, QCommandLineParser& configWindow);
