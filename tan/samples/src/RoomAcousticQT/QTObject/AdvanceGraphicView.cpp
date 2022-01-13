#include "AdvanceGraphicView.h"
#include <qdrag.h>
#include <qmimedata.h>

AdvanceGraphicView::AdvanceGraphicView(QWidget* parent) : QGraphicsView(parent)
{

}

AdvanceGraphicView::~AdvanceGraphicView()
{
}

void AdvanceGraphicView::wheelEvent(QWheelEvent* event)
{
	if (m_eZoomMode == eZoomMode_Wheel)
	{
		setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
		double scaleFactor = 1.15;

		QPoint numPixels = event->pixelDelta();
		QPoint numDegrees = event->angleDelta() / 8;

		int yDelta = 0;

		if (!numPixels.isNull()) 
		{
			yDelta = numPixels.y();
		}
		else if (!numDegrees.isNull()) 
		{
			yDelta = numDegrees.y() / 2;
		}

		if (yDelta > 0)
		{
			scale(scaleFactor, scaleFactor);
		}
		else if (yDelta < 0)
		{
			scale(1 / scaleFactor, 1 / scaleFactor);
		}
	}

	QGraphicsView::wheelEvent(event);
}

void AdvanceGraphicView::mousePressEvent(QMouseEvent* event)
{
	if (m_eZoomMode == eZoomMode_MiddleButton_Drag && event->button() == Qt::MidButton)
	{
		m_bMiddleButtonPressed = true;
	}
	QGraphicsView::mousePressEvent(event);
}

void AdvanceGraphicView::mouseReleaseEvent(QMouseEvent* event)
{
	if (m_eZoomMode == eZoomMode_MiddleButton_Drag && event->button() == Qt::MidButton)
	{
		m_bMiddleButtonPressed = false;
	}
	QGraphicsView::mouseReleaseEvent(event);
}

void AdvanceGraphicView::mouseMoveEvent(QMouseEvent* event)
{
	if (m_eZoomMode == eZoomMode_MiddleButton_Drag && m_bMiddleButtonPressed == true)
	{
		setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
		int deltaY = event->globalY() - m_iMousePositionY;
		double scaleFactor = 1.01;
		if (deltaY > 0)
		{
			scale(scaleFactor, scaleFactor);
		}
		else
		{
			scale(1 / scaleFactor, 1 / scaleFactor);
		}
	}
	m_iMousePositionY = event->globalY();
	QGraphicsView::mouseMoveEvent(event);
}
