//
//  Copyright (C) 2022 Nick Gasson
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#ifndef _SDF_PHASE_H
#define _SDF_PHASE_H

#include "prim.h"
#include "sdf-node.h"
#include "common.h"
#include "hash.h"

sdf_file_t* sdf_parse(sdf_flags_t min_max_spec);
void sdf_free(sdf_file_t *sdf);
void sdf_dump(sdf_file_t *sdf, int indent);

#endif  // _SDF_PHASE_H
