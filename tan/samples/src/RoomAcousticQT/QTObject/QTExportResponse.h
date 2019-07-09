#pragma once

#include <QtWidgets/QDialog>
#include "ui_ExportResponse.h"
#include "../RoomAcousticQT.h"

class QTExportResponse : public QDialog
{
	Q_OBJECT
public slots:
	void browseOutputFile();
	void generateResponse();
public:
	QTExportResponse(QWidget* parent, const Qt::WindowFlags& f);
	void Init(RoomAcousticQT* m_pRoomAcoustic);
private:
	void updateCurrentRoomConfig(bool in_bChecked);
	void updateCurrentListenerConfig(bool in_bChecked);
	void updateCurrentSourceConfig();
	void updateCustomRoomConfig(bool in_bChecked);
	void updateCustomListenerConfig(bool in_bChecked);
	void updateCustomSourceConfig();
	void connectSignals();
	Ui::ExportResponse m_UIExportResponse;
	RoomAcousticQT*	m_pRoomAcoustic = nullptr;
};