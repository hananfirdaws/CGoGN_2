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

#ifndef CGOGN_RENDERING_TOPO_DRAWER_H_
#define CGOGN_RENDERING_TOPO_DRAWER_H_

#include <cgogn/rendering/dll.h>

#include <cgogn/rendering/shaders/shader_simple_color.h>
#include <cgogn/rendering/shaders/shader_bold_line.h>
#include <cgogn/rendering/shaders/shader_round_point.h>
#include <cgogn/rendering/shaders/vbo.h>

#include <cgogn/geometry/algos/centroid.h>
#include <cgogn/geometry/types/geometry_traits.h>
#include <cgogn/geometry/functions/distance.h>

#include <QOpenGLFunctions_3_3_Core>
#include <QColor>

namespace cgogn
{

namespace rendering
{
/**
 * @brief Rendering of the topology
 *
 * Typical usage:
 *
 *  std::unique_ptr<cgogn::rendering::TopoDrawer> topo_;	// can be shared between contexts
 *  std::unique_ptr<cgogn::rendering::TopoDrawer::Renderer> topo_rend_; // one by context,
 *
 * init:
 *  topo_ = cgogn::make_unique<cgogn::rendering::TopoDrawer>();
 *  topo_rend_ = topo_->generate_renderer();
 *  topo_->update<Vec3>(map_,vertex_position_);
 *
 * draw:
 *  topo_rend_->draw(proj,view,this);
 *
 */
class CGOGN_RENDERING_API TopoDrawer
{
	using Vec3f = std::array<float32, 3>;

protected:

	std::unique_ptr<VBO> vbo_darts_;
	std::unique_ptr<VBO> vbo_relations_;

	QColor dart_color_;
	QColor phi2_color_;
	QColor phi3_color_;

	float32 shrink_v_;
	float32 shrink_f_;
	float32 shrink_e_;

	float32 min_length_dart_;

	std::vector<Vec3f> darts_pos_;
	std::vector<Dart> darts_id_;

	template <typename VEC3, typename MAP>
	void update_map2(const MAP& m, const typename MAP::template VertexAttribute<VEC3>& position);

	template <typename VEC3, typename MAP>
	void update_map3(const MAP& m, const typename MAP::template VertexAttribute<VEC3>& position);

public:

	class CGOGN_RENDERING_API Renderer
	{
		friend class TopoDrawer;

		std::unique_ptr<ShaderBoldLine::Param> param_bl_;
		std::unique_ptr<ShaderBoldLine::Param> param_bl2_;
		std::unique_ptr<ShaderRoundPoint::Param> param_rp_;
		TopoDrawer* topo_drawer_data_;

		Renderer(TopoDrawer* tr);

	public:

		~Renderer();

		/**
		 * @brief draw
		 * @param projection projection matrix
		 * @param modelview model-view matrix
		 * @param ogl33 OGLFunction (use "this" ptr if you inherit from QOpenGLWidget
		 * @param with_blending
		 */
		void draw(const QMatrix4x4& projection, const QMatrix4x4& modelview, QOpenGLFunctions_3_3_Core* ogl33, bool with_blending = true);

		void set_clipping_plane(const QVector4D& p);


	};

	using Self = TopoDrawer;

	/**
	 * constructor, init all buffers (data and OpenGL) and shader
	 * @Warning need OpenGL context
	 */
	TopoDrawer();

	/**
	 * release buffers and shader
	 */
	~TopoDrawer();

	CGOGN_NOT_COPYABLE_NOR_MOVABLE(TopoDrawer);

	/**
	 * @brief generate a renderer (one per context)
	 * @return pointer on renderer
	 */
	inline std::unique_ptr<Renderer> generate_renderer()
	{
		return std::unique_ptr<Renderer>(new Renderer(this));
	}

	inline void set_explode_volume(float32 x) { shrink_v_ = x; }

	inline void set_explode_face(float32 x) { shrink_f_ = x; }

	inline void set_explode_edge(float32 x) { shrink_e_ = x; }

	template <typename VEC3, typename MAP, typename std::enable_if<MAP::DIMENSION == 2>::type* = nullptr>
	void update(const MAP& m, const typename MAP::template VertexAttribute<VEC3>& position)
	{
		this->update_map2<VEC3, MAP>(m, position);
	}

	template <typename VEC3, typename MAP, typename std::enable_if<MAP::DIMENSION == 3>::type* = nullptr>
	void update(const MAP& m, const typename MAP::template VertexAttribute<VEC3>& position)
	{
		this->update_map3<VEC3, MAP>(m, position);
	}

	template <typename VEC3>
	Dart pick(const VEC3& A, const VEC3& B, VEC3& dp1, VEC3& dp2);


};



template <typename VEC3, typename MAP>
void TopoDrawer::update_map2(const MAP& m, const typename MAP::template VertexAttribute<VEC3>& position)
{
	using Vertex = typename MAP::Vertex;
	using Face = typename MAP::Face;
	using Scalar = typename geometry::vector_traits<VEC3>::Scalar;

	Scalar opp_shrink_e = 1.0 - shrink_e_;
	Scalar opp_shrink_f = 1.0 - shrink_f_;

	darts_pos_.clear();
	darts_pos_.reserve(1024 * 1024);

	darts_id_.clear();
	darts_id_.reserve(1024 * 1024);

	min_length_dart_ = std::numeric_limits<float32>::max();

	std::vector<Vec3f> out_pos2;
	out_pos2.reserve(1024 * 1024);

	std::vector<VEC3> local_vertices;
	local_vertices.reserve(256);

	std::vector<Dart> local_darts;
	local_darts.reserve(256);

	m.foreach_cell([&] (Face f)
	{
		local_vertices.clear();
		local_darts.clear();
		VEC3 center;
		center.setZero();
		uint32 count = 0u;
		m.foreach_incident_vertex(f, [&] (Vertex v)
		{
			local_vertices.push_back(position[v]);
			local_darts.push_back(v.dart);
			center += position[v];
			count++;
		});
		center /= Scalar(count);

		// phi2 mid-edge: N -> 2N-1
		for (uint32 i = 0; i < count; ++i)
			local_vertices.push_back((local_vertices[i]+local_vertices[(i+1)%count])/Scalar(2.0));

		// dart round point: 0 -> N-1
		for (uint32 i = 0; i < count; ++i)
			local_vertices[i] = local_vertices[i] * Scalar(shrink_f_) + center * (opp_shrink_f);

		//dart other extremety: 2N -> 3N-1
		for (uint32 i = 0; i < count; ++i)
			local_vertices.push_back(local_vertices[i]*(opp_shrink_e) + local_vertices[(i+1)%count]*Scalar(shrink_e_));

		//phi2 mid-dart: 3N -> 4N-1
		for (uint32 i = 0; i < count; ++i)
			local_vertices.push_back((local_vertices[i]+local_vertices[(2*count+i+1)%count])/Scalar(2.0));

		for (uint32 i = 0; i < count; ++i)
		{
			darts_id_.push_back(local_darts[i]);
			const VEC3& P1 = local_vertices[i];
			darts_pos_.push_back({float32(P1[0]), float32(P1[1]), float32(P1[2])});
			const VEC3& P2 = local_vertices[2*count+i];
			darts_pos_.push_back({float32(P2[0]), float32(P2[1]), float32(P2[2])});

			float32 l = float32((P2-P1).squaredNorm());
			if ((l>0.0f) && (l < min_length_dart_))
				min_length_dart_ = l;

			const VEC3& P3 = local_vertices[count+i];
			out_pos2.push_back({float32(P3[0]), float32(P3[1]), float32(P3[2])});
			const VEC3& P4 = local_vertices[3*count+i];
			out_pos2.push_back({float32(P4[0]), float32(P4[1]), float32(P4[2])});
		}
	});

	uint32 nbvec = std::uint32_t(darts_pos_.size());

	vbo_darts_->allocate(nbvec, 3);
	vbo_darts_->bind();
	vbo_darts_->copy_data(0, nbvec * 12, darts_pos_[0].data());
	vbo_darts_->release();

	vbo_relations_->allocate(nbvec, 3);
	vbo_relations_->bind();
	vbo_relations_->copy_data(0, nbvec * 12, out_pos2[0].data());
	vbo_relations_->release();
}

template <typename VEC3, typename MAP>
void TopoDrawer::update_map3(const MAP& m, const typename MAP::template VertexAttribute<VEC3>& position)
{
	using Vertex = typename MAP::Vertex;
	using Face = typename MAP::Face;
	using Volume = typename MAP::Volume;
	using Scalar = typename geometry::vector_traits<VEC3>::Scalar;

	Scalar opp_shrink_e = 1.0 - shrink_e_;
	Scalar opp_shrink_f = 1.0 - shrink_f_;
	Scalar opp_shrink_v = 1.0 - shrink_v_;

	darts_pos_.clear();
	darts_pos_.reserve(1024 * 1024);

	darts_id_.clear();
	darts_id_.reserve(1024 * 1024);

	min_length_dart_ = std::numeric_limits<float32>::max();

	std::vector<Vec3f> out_pos2;
	out_pos2.reserve(1024 * 1024);

	std::vector<Vec3f> out_pos3;
	out_pos3.reserve(1024 * 1024);

	std::vector<VEC3> local_vertices;
	local_vertices.reserve(256);

	std::vector<Dart> local_darts;
	local_darts.reserve(256);

	m.foreach_cell([&] (Volume v)
	{
		VEC3 center_vol = geometry::centroid<VEC3>(m, v, position);
		m.foreach_incident_face(v, [&] (Face f)
		{
			local_vertices.clear();
			local_darts.clear();
			VEC3 center;
			center.setZero();
			uint32 count = 0u;
			m.foreach_incident_vertex(f, [&] (Vertex v)
			{
				local_vertices.push_back(position[v]);
				local_darts.push_back(v.dart);
				center += position[v];
				count++;
			});
			center /= Scalar(count);

			// phi2 mid-edge: N -> 2N-1
			for (uint32 i = 0; i < count; ++i)
				local_vertices.push_back((local_vertices[i]+local_vertices[(i+1)%count])/Scalar(2.0));

			//phi3: 2N -> 3N-1
			for (uint32 i = 0; i < count; ++i)
				local_vertices.push_back(local_vertices[count+i]* shrink_f_ + center * (opp_shrink_f));

			// dart round point: 0 -> N-1
			for (uint32 i = 0; i < count; ++i)
				local_vertices[i] = local_vertices[i] * shrink_f_ + center * (opp_shrink_f);

			//dart other extremety: 3N -> 4N-1
			for (uint32 i = 0; i < count; ++i)
				local_vertices.push_back(local_vertices[i]*(opp_shrink_e) + local_vertices[(i+1)%count]*shrink_e_);

			//phi2/3 mid-dart: 4N -> 5N-1
			for (uint32 i = 0; i < count; ++i)
				local_vertices.push_back((local_vertices[i]+local_vertices[(2*count+i+1)%count])/Scalar(2.0));

			for (uint32 i = 0; i < count; ++i)
			{
				darts_id_.push_back(local_darts[i]);
				VEC3 P1 = (local_vertices[i] * shrink_v_) + (center_vol * opp_shrink_v);
				darts_pos_.push_back({float32(P1[0]), float32(P1[1]), float32(P1[2])});
				VEC3 P2 = (local_vertices[3*count+i] * shrink_v_) + (center_vol * opp_shrink_v);
				darts_pos_.push_back({float32(P2[0]), float32(P2[1]), float32(P2[2])});

				float32 l = float32((P2-P1).squaredNorm());
				if ((l>0.0f) && (l < min_length_dart_))
					min_length_dart_ = l;

				const VEC3 P3 = (local_vertices[count+i] * shrink_v_) + (center_vol * opp_shrink_v);
				out_pos2.push_back({float32(P3[0]), float32(P3[1]), float32(P3[2])});
				const VEC3 P4 = (local_vertices[4*count+i] * shrink_v_) + (center_vol * opp_shrink_v);
				out_pos2.push_back({float32(P4[0]), float32(P4[1]), float32(P4[2])});
				const VEC3& P5 = local_vertices[2*count+i];
				out_pos3.push_back({float32(P5[0]), float32(P5[1]), float32(P5[2])});
				out_pos3.push_back({float32(P4[0]), float32(P4[1]), float32(P4[2])});
			}
		});
	});

	uint32 nbvec = uint32(darts_pos_.size());
	vbo_darts_->allocate(nbvec, 3);
	vbo_darts_->bind();
	vbo_darts_->copy_data(0, nbvec * 12, darts_pos_[0].data());
	vbo_darts_->release();

	vbo_relations_->allocate(2 * nbvec, 3);
	vbo_relations_->bind();
	vbo_relations_->copy_data(0, nbvec * 12, out_pos2[0].data());
	vbo_relations_->copy_data(nbvec * 12, nbvec*12, out_pos3[0].data());

	vbo_relations_->release();
}

template <typename VEC3>
Dart TopoDrawer::pick(const VEC3& xA, const VEC3& xB, VEC3& xdp1, VEC3& xdp2)
{
	using LVEC = geometry::Vec_T<Vec3f>;

	VEC3 xAB = xB-xA;
	LVEC A(xA[0],xA[1],xA[2]);
	LVEC AB(xAB[0],xAB[1],xAB[2]);

	float32 d_seg_max = float32(min_length_dart_/10.0f);
	float32 dmax = std::numeric_limits<float32>::max();
	float32 AB2 = AB.dot(AB);

	std::size_t isel = INVALID_INDEX;

	for(std::size_t i=0, nb_d = darts_id_.size(); i<nb_d; ++i)
	{
		const Vec3f& PP = darts_pos_[2*i];
		const Vec3f& QQ = darts_pos_[2*i+1];
		const LVEC& P = reinterpret_cast<const LVEC&>(PP);
		const LVEC& Q = reinterpret_cast<const LVEC&>(QQ);
		float32 d2 =	geometry::squared_distance_line_seg(A, AB, AB2, P, Q);
		if (d2 < d_seg_max)
		{
			float32 dp = ((P+Q)/2.0f-A).squaredNorm();
			if (dp < dmax)
			{
				dp = dmax;
				isel = i;
			}
		}
	}

	if (isel != INVALID_INDEX)
	{
		Vec3f dp1 = darts_pos_[2*isel];
		Vec3f dp2 = darts_pos_[2*isel+1];
		xdp1 = VEC3(dp1[0],dp1[1],dp1[2]);
		xdp2 = VEC3(dp2[0],dp2[1],dp2[2]);
		return darts_id_[isel];
	}

	return Dart(INVALID_INDEX);

	//TODO PARALLEL: warning min -> 2 passes
}



} // namespace rendering

} // namespace cgogn

#endif // CGOGN_RENDERING_TOPO_DRAWER_H_
