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

#ifndef CGOGN_IO_SURFACE_IMPORT_H_
#define CGOGN_IO_SURFACE_IMPORT_H_

#include <istream>
#include <sstream>
#include <set>

#include <cgogn/core/utils/endian.h>
#include <cgogn/core/utils/name_types.h>
#include <cgogn/core/utils/string.h>

#include <cgogn/core/cmap/map_base_data.h>

#include <cgogn/io/dll.h>
#include <cgogn/io/c_locale.h>
#include <cgogn/io/mesh_io_gen.h>
#include <cgogn/io/data_io.h>

namespace cgogn
{

namespace io
{

template <typename VEC3>
class SurfaceImport
{
public:

	using Self = SurfaceImport<VEC3>;

	using ChunkArrayContainer = MapBaseData::ChunkArrayContainer<uint32>;
	using ChunkArrayGen = MapBaseData::ChunkArrayGen;
	template <typename T>
	using ChunkArray = MapBaseData::ChunkArray<T>;

	template <typename T, Orbit ORBIT>
	using Attribute = Attribute<T, ORBIT>;

	using DataInputGen = cgogn::io::DataInputGen;

	inline SurfaceImport():
		faces_nb_edges_()
	  ,faces_vertex_indices_()
	  ,vertex_attributes_()
	  ,face_attributes_()
	  ,position_attribute_(nullptr)
	{}

	virtual ~SurfaceImport()
	{}

	CGOGN_NOT_COPYABLE_NOR_MOVABLE(SurfaceImport);

	virtual void clear()
	{
		faces_nb_edges_.clear();
		faces_vertex_indices_.clear();
		vertex_attributes_.remove_chunk_arrays();
		face_attributes_.remove_chunk_arrays();
		position_attribute_ = nullptr;
	}

	template <typename Map>
	void create_map(Map& map)
	{
		static_assert(Map::DIMENSION == 2, "must use map of dim 2 in surface import");

		using Vertex = typename Map::Vertex;		
		using Face = typename Map::Face;
		using MapBuilder = typename Map::Builder;

		if (nb_faces() == 0u)
			return;

		MapBuilder mbuild(map);
		map.clear_and_remove_attributes();

		mbuild.template create_embedding<Vertex::ORBIT>();
		mbuild.template swap_chunk_array_container<Vertex::ORBIT>(this->vertex_attributes_);

		if (this->face_attributes_.nb_chunk_arrays() > 0)
		{
			mbuild.template create_embedding<Face::ORBIT>();
			mbuild.template swap_chunk_array_container<Face::ORBIT>(this->face_attributes_);
		}

		auto darts_per_vertex = map.template add_attribute<std::vector<Dart>, Vertex>("darts_per_vertex");

		uint32 faces_vertex_index = 0;
		std::vector<uint32> vertices_buffer;
		vertices_buffer.reserve(16);
		uint32 face_emb = 0u;

		for (uint32 i = 0, end = nb_faces(); i < end; ++i)
		{
			uint32 nbe = this->faces_nb_edges_[i];

			vertices_buffer.clear();
			uint32 prev = std::numeric_limits<uint32>::max();

			for (uint32 j = 0; j < nbe; ++j)
			{
				uint32 idx = this->faces_vertex_indices_[faces_vertex_index++];
				if (idx != prev)
				{
					prev = idx;
					vertices_buffer.push_back(idx);
				}
			}
			if (vertices_buffer.front() == vertices_buffer.back())
				vertices_buffer.pop_back();

			nbe = static_cast<unsigned short>(vertices_buffer.size());
			if (nbe > 2)
			{
				Dart d = mbuild.add_face_topo_fp(nbe);
				for (uint32 j = 0u; j < nbe; ++j)
				{
					const uint32 vertex_index = vertices_buffer[j];
					mbuild.template set_embedding<Vertex>(d, vertex_index);
					darts_per_vertex[vertex_index].push_back(d);
					d = map.phi1(d);
				}
				if (map.is_embedded(Face::ORBIT))
					mbuild. template set_orbit_embedding<Face>(Face(d), face_emb++);
			}
		}

		bool need_vertex_unicity_check = false;
		uint32 nb_boundary_edges = 0;

		map.foreach_dart([&] (Dart d)
		{
			if (map.phi2(d) == d)
			{
				uint32 vertex_index = map.embedding(Vertex(d));

				std::vector<Dart>& next_vertex_darts = darts_per_vertex[Vertex(map.phi1(d))];
				bool phi2_found = false;
				bool first_OK = true;

				for (auto it = next_vertex_darts.begin();
					 it != next_vertex_darts.end() && !phi2_found;
					 ++it)
				{
					if (map.embedding(Vertex(map.phi1(*it))) == vertex_index)
					{
						if (map.phi2(*it) == *it)
						{
							mbuild.phi2_sew(d, *it);
							phi2_found = true;
						}
						else
						{
							first_OK = false;
						}
					}
				}

				if (!phi2_found)
					++nb_boundary_edges;

				if (!first_OK)
					need_vertex_unicity_check = true;
			}
		});

		if (nb_boundary_edges > 0)
		{
			uint32 nb_holes = mbuild.close_map();
			cgogn_log_info("create_map") << nb_holes << " hole(s) have been closed";
		}

		if (need_vertex_unicity_check)
		{
			map.template enforce_unique_orbit_embedding<Vertex::ORBIT>();
			cgogn_log_warning("create_map") << "Import Surface: non manifold vertices detected and corrected";
		}



		map.remove_attribute(darts_per_vertex);
		this->clear();
	}

	inline void add_vertex_attribute(const DataInputGen& in_data, const std::string& att_name)
	{
		ChunkArrayGen* att = in_data.add_attribute(vertex_attributes_, att_name);
		in_data.to_chunk_array(att);
		if (att_name == "position")
			position_attribute_ = dynamic_cast<ChunkArray<VEC3>*>(att);
	}

	template<typename T>
	inline ChunkArray<T>* add_vertex_attribute(const std::string& att_name)
	{
		return vertex_attributes_.template add_chunk_array<T>(att_name);
	}

	inline void add_face_attribute(const DataInputGen& in_data, const std::string& att_name)
	{
		in_data.to_chunk_array(in_data.add_attribute(face_attributes_, att_name));
	}

	inline ChunkArray<VEC3>* position_attribute()
	{
		if (position_attribute_ == nullptr)
			return (position_attribute_ =  add_vertex_attribute<VEC3>("position"));
		else
			return position_attribute_;
	}

	inline uint32 insert_line_vertex_container()
	{
		return vertex_attributes_.template insert_lines<1>();
	}

	inline uint32 insert_line_face_container()
	{
		return face_attributes_.template insert_lines<1>();
	}

	inline void reserve(uint32 nb_faces)
	{
		faces_nb_edges_.reserve(nb_faces);
		faces_vertex_indices_.reserve(nb_faces * 4u);
	}

	void add_triangle(uint32 p0, uint32 p1, uint32 p2)
	{
		faces_nb_edges_.push_back(3);
		faces_vertex_indices_.push_back(p0);
		faces_vertex_indices_.push_back(p1);
		faces_vertex_indices_.push_back(p2);
	}

	void add_quad(uint32 p0, uint32 p1, uint32 p2, uint32 p3)
	{
		faces_nb_edges_.push_back(4);
		faces_vertex_indices_.push_back(p0);
		faces_vertex_indices_.push_back(p1);
		faces_vertex_indices_.push_back(p2);
		faces_vertex_indices_.push_back(p3);
	}

	void add_face(const std::vector<uint32>& v_ids)
	{
		faces_nb_edges_.push_back(uint32(v_ids.size()));
		for (uint32 id : v_ids)
			faces_vertex_indices_.push_back(id);
	}

private:

	inline uint32 nb_faces() const
	{
		return uint32(faces_nb_edges_.size());
	}

protected:

	std::vector<uint32> faces_nb_edges_;
	std::vector<uint32> faces_vertex_indices_;

	ChunkArrayContainer vertex_attributes_;
	ChunkArrayContainer face_attributes_;
	ChunkArray<VEC3>*	position_attribute_;
};

template <typename VEC3>
class SurfaceFileImport : public SurfaceImport<VEC3>, public FileImport
{
	using Self = SurfaceFileImport<VEC3>;
	using Inherit1 = SurfaceImport<VEC3>;
	using Inherit2 = FileImport;

	CGOGN_NOT_COPYABLE_NOR_MOVABLE(SurfaceFileImport);

public:

	inline SurfaceFileImport() : Inherit1(), Inherit2()
	{}

	virtual ~SurfaceFileImport()
	{}
};

#if defined(CGOGN_USE_EXTERNAL_TEMPLATES) && (!defined(CGOGN_IO_SURFACE_IMPORT_CPP_))
extern template class CGOGN_IO_API SurfaceImport<Eigen::Vector3f>;
extern template class CGOGN_IO_API SurfaceImport<Eigen::Vector3d>;
extern template class CGOGN_IO_API SurfaceFileImport<Eigen::Vector3f>;
extern template class CGOGN_IO_API SurfaceFileImport<Eigen::Vector3d>;
#endif // defined(CGOGN_USE_EXTERNAL_TEMPLATES) && (!defined(CGOGN_IO_SURFACE_IMPORT_CPP_))

} // namespace io

} // namespace cgogn

#endif // CGOGN_IO_SURFACE_IMPORT_H_
