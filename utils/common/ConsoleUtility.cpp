#include "ConsoleUtility.h"

void initFromCommandLine(QApplication &application, QCommandLineParser &consoleOptions)
{
	consoleOptions.addHelpOption();

	QCommandLineOption configFile("configFile", "Initialize config with <File>", "File");
	QCommandLineOption metricsFile("metricsFile", "Save all gathered metrics into <File>", "File");
	QCommandLineOption playDuration("playDuration", "Sets duration of simulation to <Time> in seconds", "Time");
	QCommandLineOption outputFile("outputFile", "Save RoomAcousticsQT output into <File>", "File");
	QCommandLineOption restartCounter("restartCounter", "Sets restart counter <RestartCount> - 1,2,...", "RestartCount");

	
	consoleOptions.addOption(configFile);
	consoleOptions.addOption(metricsFile);
	consoleOptions.addOption(playDuration);
	consoleOptions.addOption(outputFile);
	consoleOptions.addOption(restartCounter);
	consoleOptions.process(application);

	std::cout << "Console mode initiated:" << std::endl
				<< "Config file path: " << consoleOptions.value(configFile).toStdString() << std::endl
				<< "Output file path: " << consoleOptions.value(outputFile).toStdString() << std::endl
				<< "Play duration in seconds: " << consoleOptions.value(playDuration).toInt() << std::endl
				<< "Metrics file path: " << consoleOptions.value(metricsFile).toStdString() << std::endl
				<< "Restart count number: " << consoleOptions.value(restartCounter).toInt() << std::endl;

	if (consoleOptions.isSet("restartCounter") && consoleOptions.isSet("playDuration"))
	{
		std::cerr << "Console mode error: conflicting options restartCounter and playDuration!" << std::endl;
	}
}
