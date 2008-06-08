/*
    Scan Tailor - Interactive post-processing tool for scanned pages.
    Copyright (C) 2007-2008  Joseph Artsimovich <joseph_a@mail.ru>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "PageLayout.h"
#include "XmlMarshaller.h"
#include "XmlUnmarshaller.h"
#include <QPolygonF>
#include <QSizeF>
#include <QPointF>
#include <QTransform>
#include <QDomElement>
#include <QDomDocument>
#include <algorithm>
#include <math.h>

PageLayout::PageLayout()
:	m_leftPageValid(false),
	m_rightPageValid(false)
{
}

PageLayout::PageLayout(QLineF const& split_line,
	bool left_page_valid, bool right_page_valid)
:	m_splitLine(split_line),
	m_leftPageValid(left_page_valid),
	m_rightPageValid(right_page_valid)
{
}

PageLayout::PageLayout(QDomElement const& layout_el)
:	m_splitLine(
		XmlUnmarshaller::lineF(
			layout_el.namedItem("split-line").toElement()
		)
	),
	m_leftPageValid(layout_el.attribute("leftPageValid") == "1"),
	m_rightPageValid(layout_el.attribute("rightPageValid") == "1")
{
}

int
PageLayout::numSubPages() const
{
	int cnt = 0;
	if (m_leftPageValid) {
		++cnt;
	}
	if (m_rightPageValid) {
		++cnt;
	}
	return cnt;
}

QLineF
PageLayout::inscribedSplitLine(QRectF const& rect) const
{
#if 0 // QPolygon::intersected() is buggy in Qt < 4.3.3
	if (m_splitLine.isNull()) {
		return QLineF();
	}
	
	QLineF const top_line(rect.topLeft(), rect.topRight());
	QLineF const bottom_line(rect.bottomLeft(), rect.bottomRight());
	
	QPointF p1, p2;
	m_splitLine.intersect(top_line, &p1);
	m_splitLine.intersect(bottom_line, &p2);
	
	QPolygonF poly;
	poly.push_back(p1);
	poly.push_back(p2);
	
	QPolygonF const intersected(poly.intersected(rect));
	
	if (intersected.size() < 2) {
		return QLineF();
	}
	
	return QLineF(intersected[0], intersected[1]);
#else
	QLineF line(m_splitLine);
	if (fabs(line.p2().x() - line.p1().x()) <
	    fabs(line.p2().y() - line.p1().y())) {
		line = intersectTopBottom(line, rect.top(), rect.bottom());
		line = clipLeftRight(line, rect.left(), rect.right());
	} else {
		line = intersectLeftRight(line, rect.left(), rect.right());
		line = clipTopBottom(line, rect.top(), rect.bottom());
	}
	return line;
#endif
}

QPolygonF
PageLayout::leftPage(QRectF const& rect) const
{
	if (m_splitLine.p1().y() == m_splitLine.p2().y()) {
		return QPolygonF();
	}
	
	QLineF const top_line(rect.topLeft(), rect.topRight());
	QLineF const bottom_line(rect.bottomLeft(), rect.bottomRight());
	
	QPointF p_top;
	QPointF p_bottom;
	m_splitLine.intersect(top_line, &p_top);
	m_splitLine.intersect(bottom_line, &p_bottom);
	
	double const left = std::min(
		rect.left() - 1, std::min(p_top.x(), p_bottom.x())
	);
	// Why - 1?  Because QPolygonF::intersected(QRectF) doesn't work
	// right (as of Qt 4.3.1) when the polygon is exactly equal
	// to the rect.
	
	QPolygonF poly;
	poly.push_back(QPointF(left, rect.top()));
	poly.push_back(QPointF(p_top.x(), rect.top()));
	poly.push_back(QPointF(p_bottom.x(), rect.bottom()));
	poly.push_back(QPointF(left, rect.bottom()));
	poly.push_back(QPointF(left, rect.top()));
	return poly.intersected(rect);
}

QPolygonF
PageLayout::rightPage(QRectF const& rect) const
{
	if (m_splitLine.p1().y() == m_splitLine.p2().y()) {
		return QPolygonF();
	}
	
	QLineF const top_line(rect.topLeft(), rect.topRight());
	QLineF const bottom_line(rect.bottomLeft(), rect.bottomRight());
	
	QPointF p_top;
	QPointF p_bottom;
	m_splitLine.intersect(top_line, &p_top);
	m_splitLine.intersect(bottom_line, &p_bottom);
	
	QPolygonF poly;
	double const right = std::max(
		rect.right() + 1, std::max(p_top.x(), p_bottom.x())
	);
	// Why + 1?  Because QPolygonF::intersected(QRectF) doesn't work
	// right (as of Qt 4.3.1) when the polygon is exactly equal
	// to the rect.
	
	poly.push_back(QPointF(right, rect.top()));
	poly.push_back(QPointF(p_top.x(), rect.top()));
	poly.push_back(QPointF(p_bottom.x(), rect.bottom()));
	poly.push_back(QPointF(right, rect.bottom()));
	poly.push_back(QPointF(right, rect.top()));
	return poly.intersected(rect);
}

QPolygonF
PageLayout::pageOutline(QRectF const& rect, LogicalPageId::SubPage const page) const
{
	switch (page) {
		case LogicalPageId::LEFT_PAGE:
			return leftPage(rect);
		case LogicalPageId::RIGHT_PAGE:
			return rightPage(rect);
		default:;
	}
	
	if (m_leftPageValid) {
		return leftPage(rect);
	} else if (m_rightPageValid) {
		return rightPage(rect);
	} else {
		return QPolygonF();
	}
}

PageLayout
PageLayout::transformed(QTransform const& xform) const
{
	QLineF const line(xform.map(m_splitLine));
	return PageLayout(line, m_leftPageValid, m_rightPageValid);
}

QDomElement
PageLayout::toXml(QDomDocument& doc, QString const& name) const
{
	if (isNull()) {
		return QDomElement();
	}
	
	XmlMarshaller marshaller(doc);
	
	QDomElement el(doc.createElement(name));
	el.setAttribute("leftPageValid", m_leftPageValid ? 1 : 0);
	el.setAttribute("rightPageValid", m_rightPageValid ? 1 : 0);
	el.appendChild(marshaller.lineF(m_splitLine, "split-line"));
	
	return el;
}

QLineF
PageLayout::intersectLeftRight(QLineF const& line, double const x_left, double const x_right)
{
	if (line.p1().x() == line.p2().x()) {
		// This will prevent division by zero and also catch null lines.
		return line;
	}
	
	// y = p1.y() + (x - p1.x()) * tg;
	// x = p1.x() + (y - p1.y()) * ctg;
	
	double const tg = (line.p2().y() - line.p1().y()) / (line.p2().x() - line.p1().x());
	double const y_left = line.p1().y() + (x_left - line.p1().x()) * tg;
	double const y_right = line.p1().y() + (x_right - line.p1().x()) * tg;
	
	return QLineF(x_left, y_left, x_right, y_right);
}

QLineF
PageLayout::intersectTopBottom(QLineF const& line, double const y_top, double const y_bottom)
{
	if (line.p1().y() == line.p2().y()) {
		// This will prevent division by zero and also catch null lines.
		return line;
	}
	
	// y = p1.y() + (x - p1.x()) * tg;
	// x = p1.x() + (y - p1.y()) * ctg;
	
	double const ctg = (line.p2().x() - line.p1().x()) / (line.p2().y() - line.p1().y());
	double const x_top = line.p1().x() + (y_top - line.p1().y()) * ctg;
	double const x_bottom = line.p1().x() + (y_bottom - line.p1().y()) * ctg;
	
	return QLineF(x_top, y_top, x_bottom, y_bottom);
}

QLineF
PageLayout::clipLeftRight(QLineF const& line, double x_left, double x_right)
{
	QPointF p_left(line.p1());
	QPointF p_right(line.p2());
	if (p_left.x() > p_right.x()) {
		std::swap(p_left, p_right);
	} else if (p_left.x() == p_right.x()) {
		// This will prevent division by zero and also catch null lines.
		return line;
	}
	
	if (p_right.x() < x_left || p_left.x() > x_right) {
		return QLineF();
	}
	
	// y = p1.y() + (x - p1.x()) * tg;
	// x = p1.x() + (y - p1.y()) * ctg;
	
	double const tg = (line.p2().y() - line.p1().y()) / (line.p2().x() - line.p1().x());
	
	if (p_left.x() < x_left) {
		p_left.setY(line.p1().y() + (x_left - line.p1().x()) * tg);
		p_left.setX(x_left);
	}
	
	if (p_right.x() > x_right) {
		p_right.setY(line.p1().y() + (x_right - line.p1().x()) * tg);
		p_right.setX(x_right);
	}
	
	return QLineF(p_left, p_right);
}

QLineF
PageLayout::clipTopBottom(QLineF const& line, double y_top, double y_bottom)
{
	QPointF p_top(line.p1());
	QPointF p_bottom(line.p2());
	if (p_top.y() > p_bottom.y()) {
		std::swap(p_top, p_bottom);
	} else if (p_top.y() == p_bottom.y()) {
		// This will prevent division by zero and also catch null lines.
		return line;
	}
	
	if (p_bottom.y() < y_top || p_top.y() > y_bottom) {
		return QLineF();
	}
	
	// y = p1.y() + (x - p1.x()) * tg;
	// x = p1.x() + (y - p1.y()) * ctg;
	
	double const ctg = (line.p2().x() - line.p1().x()) / (line.p2().y() - line.p1().y());
	
	if (p_top.y() < y_top) {
		p_top.setX(line.p1().x() + (y_top - line.p1().y()) * ctg);
		p_top.setY(y_top);
	}
	
	if (p_bottom.y() > y_bottom) {
		p_bottom.setX(line.p1().x() + (y_bottom - line.p1().y()) * ctg);
		p_bottom.setY(y_bottom);
	}
	
	return QLineF(p_top, p_bottom);
}

