#pragma once
#include <QCommandLineParser>
#include <iostream>
#include <QtWidgets/QApplication>
#include "QTRoomAcousticConfig.h"

void initFromCommandLine(QApplication& application, QCommandLineParser& configWindow);
