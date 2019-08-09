/*

Copyright (C) 2007-2019 John W. Eaton

This file is part of Octave.

Octave is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Octave is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Octave; see the file COPYING.  If not, see
<https://www.gnu.org/licenses/>.

*/

#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#include "graphics.h"
#include "gtk-manager.h"
#include "interpreter-private.h"

void
base_graphics_toolkit::update (const graphics_handle& h, int id)
{
  gh_manager& gh_mgr
    = octave::__get_gh_manager__ ("base_graphics_toolkit::update");

  graphics_object go = gh_mgr.get_object (h);

  update (go, id);
}

bool
base_graphics_toolkit::initialize (const graphics_handle& h)
{
  gh_manager& gh_mgr
    = octave::__get_gh_manager__ ("base_graphics_toolkit::initialize");

  graphics_object go = gh_mgr.get_object (h);

  return initialize (go);
}
