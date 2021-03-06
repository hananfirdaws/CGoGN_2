/*******************************************************************************
* CGoGN: Combinatorial and Geometric modeling with Generic N-dimensional Maps  *
* Copyright (C) 2015, IGG Group, ICube, University of Strasbourg, France       *
*                                                                              *
* This library is free software; you can redistribute it and/or modify it      *
* under the terms of the GNU Lesser General Public License as published by the *
* Free Software Foundation; either version 2.1 of the License, or (at your     *
* option) any later version.                                                   *
*                                                                              *
* This library is distributed in the hope that it will be useful, but WITHOUT  *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License  *
* for more details.                                                            *
*                                                                              *
* You should have received a copy of the GNU Lesser General Public License     *
* along with this library; if not, write to the Free Software Foundation,      *
* Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.           *
*                                                                              *
* Web site: http://cgogn.unistra.fr/                                           *
* Contact information: cgogn@unistra.fr                                        *
*                                                                              *
*******************************************************************************/

#define CGOGN_RENDERING_TRANSP_VOLUME_RENDER_CPP_

#include <cgogn/rendering/transparency_volume_drawer.h>

#include <QOpenGLFunctions>
#include <iostream>
#include<QColor>
#include<QImage>

namespace cgogn
{

namespace rendering
{

VolumeTransparencyDrawer::VolumeTransparencyDrawer() :
	vbo_pos_(nullptr),
	face_color_(0,150,0),
	shrink_v_(0.6f)
{
	vbo_pos_ = cgogn::make_unique<cgogn::rendering::VBO>(3);
}



VolumeTransparencyDrawer::Renderer::~Renderer()
{
	param_transp_vol_.reset();
	param_trq_.reset();
	fbo_layer_.reset();
	if (ogl33_)
		ogl33_->glDeleteQueries(1, &oq_transp_);
}


VolumeTransparencyDrawer::Renderer::Renderer(VolumeTransparencyDrawer* vr) :
	param_transp_vol_(nullptr),
	volume_drawer_data_(vr),
	param_trq_(nullptr),
	max_nb_layers_(8),
	fbo_layer_(nullptr),
	oq_transp_(0u),
	ogl33_(nullptr),
	depthTexture_(0)
{
	param_transp_vol_ = ShaderTransparentVolumes::generate_param();
	param_transp_vol_->set_position_vbo(vr->vbo_pos_.get());
	param_transp_vol_->explode_factor_ = vr->shrink_v_;
	param_transp_vol_->color_ = vr->face_color_;

	param_trq_ = ShaderTranspQuad::generate_param();

	param_copy_depth_ = ShaderCopyDepth::generate_param();

}


void VolumeTransparencyDrawer::Renderer::resize(int w, int h, QOpenGLFunctions_3_3_Core* ogl33)
{
	width_ = w;
	height_ = h;
	ogl33_ = ogl33;

	fbo_layer_= cgogn::make_unique<QOpenGLFramebufferObject>(width_,height_,QOpenGLFramebufferObject::Depth,GL_TEXTURE_2D,GL_RGBA32F);
	fbo_layer_->addColorAttachment(width_,height_,GL_RGBA32F);
	fbo_layer_->addColorAttachment(width_,height_,GL_R32F);
	fbo_layer_->addColorAttachment(width_,height_,GL_R32F);
	fbo_layer_->addColorAttachment(width_,height_,GL_R32F); // first depth
	fbo_layer_->addColorAttachment(width_, height_);// for depth render pass

	if (! ogl33->glIsQuery(oq_transp_))
		ogl33->glGenQueries(1, &oq_transp_);

	if (ogl33->glIsTexture(depthTexture_))
		ogl33->glDeleteTextures(1, &depthTexture_);

	ogl33->glGenTextures(1, &depthTexture_);
	ogl33->glBindTexture(GL_TEXTURE_2D, depthTexture_);
	ogl33->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	ogl33->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	ogl33->glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	ogl33->glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	ogl33->glTexImage2D( GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width_, height_, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
}


void VolumeTransparencyDrawer::Renderer::draw_faces(const QMatrix4x4& projection, const QMatrix4x4& modelview)
{
	GLfloat bkColor[4];
	ogl33_->glGetFloatv(GL_COLOR_CLEAR_VALUE, bkColor);

	ogl33_->glEnable(GL_TEXTURE_2D);
	ogl33_->glBindTexture(GL_TEXTURE_2D, depthTexture_);
	ogl33_->glReadBuffer(GL_BACK);
	ogl33_->glCopyTexSubImage2D(GL_TEXTURE_2D,0,0,0,0,0, width_, height_);

	QVector<GLuint> textures = fbo_layer_->textures();
	GLenum attach_buffs[4] = { GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT5 };

	param_transp_vol_->rgba_texture_sampler_ = 0;
	param_transp_vol_->depth_texture_sampler_ = 1;

	fbo_layer_->bind();

	// initial clears
	ogl33_->glDrawBuffers(1, attach_buffs+2);
	ogl33_->glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	ogl33_->glClear(GL_COLOR_BUFFER_BIT);
	ogl33_->glDrawBuffers(1, attach_buffs);
	ogl33_->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	for (int p = 0; p<max_nb_layers_; ++p)
	{
		ogl33_->glClear(GL_DEPTH_BUFFER_BIT);

		// copy initial Zbuffer to begin each pass
		if (p > 0)
		{
			ogl33_->glDrawBuffers(1, attach_buffs+3);
			param_copy_depth_->bind(projection, modelview);
			ogl33_->glBindTexture(GL_TEXTURE_2D, depthTexture_);
			ogl33_->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
			param_copy_depth_->release();
		}

		ogl33_->glDrawBuffers(2, attach_buffs);
		param_transp_vol_->layer_ = p;
		ogl33_->glActiveTexture(GL_TEXTURE0);
		ogl33_->glBindTexture(GL_TEXTURE_2D, textures[1]);
		ogl33_->glActiveTexture(GL_TEXTURE1);
		ogl33_->glBindTexture(GL_TEXTURE_2D, textures[3]);

		ogl33_->glBeginQuery(GL_SAMPLES_PASSED, oq_transp_);

		param_transp_vol_->bind(projection, modelview);
		ogl33_->glDrawArrays(GL_LINES_ADJACENCY, 0, volume_drawer_data_->vbo_pos_->size());
		param_transp_vol_->release();

		ogl33_->glEndQuery(GL_SAMPLES_PASSED);

		GLuint nb_samples;
		ogl33_->glGetQueryObjectuiv(oq_transp_, GL_QUERY_RESULT, &nb_samples);

		if (nb_samples == 0) // finished ?
		{
			p = max_nb_layers_;
		}
		else
		{
			ogl33_->glReadBuffer(GL_COLOR_ATTACHMENT2);
			ogl33_->glBindTexture(GL_TEXTURE_2D, textures[3]);
			ogl33_->glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, 0, 0, width_, height_, 0);

			if (p == 0)
			{
				ogl33_->glBindTexture(GL_TEXTURE_2D, textures[4]);
				ogl33_->glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, 0, 0, width_, height_, 0);
			}

			ogl33_->glReadBuffer(GL_COLOR_ATTACHMENT0);
			ogl33_->glBindTexture(GL_TEXTURE_2D, textures[1]);
			ogl33_->glCopyTexImage2D(GL_TEXTURE_2D, 0,GL_RGBA32F, 0, 0, width_, height_, 0);
		}
	}

	fbo_layer_->release();

	// real draw with blending with opaque object

	param_trq_->rgba_texture_sampler_ = 0;
	param_trq_->depth_texture_sampler_ = 1;

	ogl33_->glActiveTexture(GL_TEXTURE0);
	ogl33_->glBindTexture(GL_TEXTURE_2D, textures[1]);

	ogl33_->glActiveTexture(GL_TEXTURE1);
	ogl33_->glBindTexture(GL_TEXTURE_2D, textures[4]);

	ogl33_->glEnable(GL_BLEND);
	ogl33_->glBlendFunc(GL_ONE, GL_SRC_ALPHA);
	param_trq_->bind(projection, modelview);
	ogl33_->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	param_trq_->release();
	ogl33_->glDisable(GL_BLEND);

	ogl33_->glClearColor(bkColor[0],bkColor[1],bkColor[2],bkColor[3]);

}




void VolumeTransparencyDrawer::Renderer::set_explode_volume(float32 x)
{
	if (param_transp_vol_)
		param_transp_vol_->explode_factor_ = x;
}

void VolumeTransparencyDrawer::Renderer::set_color(const QColor& rgb)
{
	if (param_transp_vol_)
		param_transp_vol_->color_ = rgb;
}

void VolumeTransparencyDrawer::Renderer::set_clipping_plane(const QVector4D& pl)
{
	if (param_transp_vol_)
		param_transp_vol_->plane_clip_ = pl;
}

void VolumeTransparencyDrawer::Renderer::set_clipping_plane2(const QVector4D& pl)
{
	if (param_transp_vol_)
		param_transp_vol_->plane_clip2_ = pl;
}

void VolumeTransparencyDrawer::Renderer::set_thick_clipping_plane(const QVector4D& p, float32 th)
{
	QVector4D p1 = p;
	p1[3] -= th/2.0f;
	set_clipping_plane(p1);

	QVector4D p2 = -p;
	p2[3] -= th/2.0f;
	set_clipping_plane2(p2);
}

void VolumeTransparencyDrawer::Renderer::set_back_face_culling(bool cull)
{
	param_transp_vol_->bf_culling_ = cull;
}

void VolumeTransparencyDrawer::Renderer::set_lighted(bool lighted)
{
	param_transp_vol_->lighted_=lighted;
}

void VolumeTransparencyDrawer::Renderer::set_max_nb_layers(int nbl)
{
	max_nb_layers_ = nbl;
}




} // namespace rendering

} // namespace cgogn
