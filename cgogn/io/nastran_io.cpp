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

#define CGOGN_IO_NASTRAN_IO_CPP_

#include <cgogn/io/nastran_io.h>

namespace cgogn
{

namespace io
{

template class CGOGN_IO_API NastranIO<Eigen::Vector3d>;
template class CGOGN_IO_API NastranIO<Eigen::Vector3f>;
template class CGOGN_IO_API NastranIO<geometry::Vec_T<std::array<float64,3>>>;
template class CGOGN_IO_API NastranIO<geometry::Vec_T<std::array<float32,3>>>;

template class CGOGN_IO_API NastranVolumeImport<Eigen::Vector3d>;
template class CGOGN_IO_API NastranVolumeImport<Eigen::Vector3f>;
template class CGOGN_IO_API NastranVolumeImport<geometry::Vec_T<std::array<float64,3>>>;
template class CGOGN_IO_API NastranVolumeImport<geometry::Vec_T<std::array<float32,3>>>;

template class CGOGN_IO_API NastranVolumeExport<CMap3>;

} // namespace io

} // namespace cgogn
