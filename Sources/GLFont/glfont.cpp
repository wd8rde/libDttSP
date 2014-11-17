/*
 * This file is part of ExpertSDR
 *
 * ExpertSDR is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * ExpertSDR is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Radio; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 *
 *
 * Copyright (C) 2014 Tsukihime
 */

#include "glfont.h"

#define GL_CLAMP_TO_EDGE 0x812F

int GLFont::calc_texture_side(const QFont &fnt)
{
	QFontMetrics fmetrics(fnt);
	QString str;

	int rect_w = 0;
	int rect_h = fmetrics.height() * 16;

	for(int i = 0; i < 16; i++)
	{
		str.clear();
		for(int k = 0; k < 16; k++)
		{
			str.append(QChar::fromAscii(16 * i + k));
			int w = fmetrics.width(str);
			if(rect_w < w)
				rect_w = w;
		}
	}

	int texture_side = (rect_h > rect_w) ? rect_h : rect_w;
	int texture_side_pow2 = 1;

	while(texture_side != 0)
	{
		texture_side = (texture_side >> 1);
		texture_side_pow2 = texture_side_pow2 * 2;
	}
	return texture_side_pow2;
}

QImage GLFont::create_font_img(const QFont &fnt, int texture_side)
{
	QImage img(texture_side, texture_side, QImage::Format_ARGB32_Premultiplied);

	QPainter p(&img);
	p.setFont(fnt);
	p.setBackground(QBrush(QColor(0, 0, 0, 0)));
	p.eraseRect(img.rect());

	QFontMetrics fm(fnt);

	int pos_y = 0;
	for(int i = 0; i < 16; i++)
	{
		pos_y += fm.height() + 2;
		int pos_x = 0;
		for(int k = 0; k < 16; k++)
		{
			unsigned char idx = 16 * i + k;
			QChar ch(idx);
			int ch_w = fm.width(ch) + 2; // -1..1 px outline
			int ch_h = fm.height() + 2; // -1..1 px outline

			QRectF r;
			r.setTop(pos_y - ch_h);
			r.setLeft(pos_x);
			r.setWidth(ch_w);
			r.setHeight(ch_h);
			glyphs[idx].rect = r;


			int txt_x = pos_x;
			int txt_y = pos_y - fm.descent() - fm.underlinePos() - 1;

			// 1 px outline
			p.setPen(QPen(Qt::black));
			for(int a = 0; a < 3; a++)
				for(int b = 0; b < 3; b++)
					p.drawText(txt_x + a, txt_y + b, ch);

			p.setPen(QPen(Qt::white));
			p.drawText(txt_x + 1, txt_y + 1, ch);

			//p.setPen(QPen(Qt::red));
			//p.drawRect(r);

			pos_x += ch_w + 2;
		}
	}
	p.end();

	//img.save("lo.png", "PNG");

	return img;
}

GLuint GLFont::create_font_texture(QImage &font_img)
{
	GLuint texture;
	QImage img = QGLWidget::convertToGLFormat(font_img);

	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLdouble)img.width(), (GLdouble)img.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, img.bits());

	return texture;
}

void GLFont::fill_call_lists(int tex_side)
{
	for(int i = 0; i < 256; i++)
	{
		QRectF r = glyphs[i].rect;

		GLdouble px_h = r.height();
		GLdouble px_w = r.width();

		GLdouble ch_x1 = r.left() / tex_side;
		GLdouble ch_y1 = 1 - r.bottom() / tex_side; // 1- means convert y coord from top = 0 to bottom = 0
		GLdouble ch_x2 = r.right() / tex_side;
		GLdouble ch_y2 = 1 - r.top() / tex_side;

		glyphs[i].list = glGenLists(1);

		glNewList(glyphs[i].list, GL_COMPILE);

		glBegin(GL_QUADS);
		glTexCoord2d(ch_x1, ch_y1); glVertex2d(0, 0);
		glTexCoord2d(ch_x1, ch_y2); glVertex2d(0, px_h);
		glTexCoord2d(ch_x2, ch_y2); glVertex2d(px_w, px_h);
		glTexCoord2d(ch_x2, ch_y1); glVertex2d(px_w, 0);
		glEnd();

		glTranslated(px_w - 2, 0, 0); // -2 = outline hack

		glEndList();
	}
}

GLFont::GLFont(QGLWidget *parent, const QFont &fnt)
{
	this->parent = parent;
	tex_fnt = fnt;

	QFontMetrics fm(fnt);
	px_font_descent = fm.descent() + 1;

	int texture_side = calc_texture_side(fnt);
	QImage img = create_font_img(fnt, texture_side);
	font_texture = create_font_texture(img);
	fill_call_lists(texture_side);
}

GLFont::~GLFont()
{
	glDeleteTextures(1, &font_texture);
	for(int i = 0; i < 256; i++)
	{
		glDeleteLists(glyphs[i].list, 1);
	}
}

void GLFont::draw(GLfloat x, GLfloat y, GLfloat z, const QString &str)
{
	draw(x, y, z, str, Qt::white);
}

void GLFont::draw(GLfloat x, GLfloat y, GLfloat z, const QString &str, const QColor &col)
{
	GLdouble old_col[4];
	glGetDoublev(GL_CURRENT_COLOR, &old_col[0]);

	qreal r,g,b,a;
	col.getRgbF(&r, &g, &b, &a);
	glColor4f(r, g, b, a);

	GLdouble screen_scale_x = 2.0 / parent->width();
	GLdouble screen_scale_y = 2.0 / parent->height();

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();

	glEnable(GL_TEXTURE_2D);
	glEnable(GL_ALPHA_TEST);

	//glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glBindTexture(GL_TEXTURE_2D, font_texture);
	//----------------------
	glTranslated(x, y, z);

	// some magic for scaling
	// font dont be scale
	GLdouble matrix[4][4];
	glGetDoublev(GL_MODELVIEW_MATRIX, &matrix[0][0]);

	GLdouble current_scale_x = matrix[0][0];
	GLdouble current_scale_y = matrix[1][1];

	GLdouble scale_x = screen_scale_x/current_scale_x;
	GLdouble scale_y = screen_scale_y/current_scale_y;

	glScaled(scale_x, scale_y, 0);

	glTranslated(0, -px_font_descent, 0);

	glDisable(GL_DEPTH_TEST);
	for(int i = 0; i < str.size(); i++)
	{
		int ch = (int)str.toAscii().at(i);
		glCallList(glyphs[ch].list);
	}
	glEnable(GL_DEPTH_TEST);

	//----------------------
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_TEXTURE_2D);
	glPopMatrix();

	glColor4dv(&old_col[0]);
}

QFont GLFont::font() const
{
	return tex_fnt;
}
