// -*- Mode: C++; tab-width:2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vi:tw=80:et:ts=2:sts=2
//
// -----------------------------------------------------------------------
//
// This file is part of RLVM, a RealLive virtual machine clone.
//
// -----------------------------------------------------------------------
//
// Copyright (C) 2008 Elliot Glaysher
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
//
// -----------------------------------------------------------------------

#ifndef SRC_SYSTEMS_BASE_MOUSECURSOR_HPP_
#define SRC_SYSTEMS_BASE_MOUSECURSOR_HPP_

#include <boost/shared_ptr.hpp>
#include "Systems/Base/Rect.hpp"

class Surface;
class RLMachine;

// Represents a mouse cursor on screen.
class MouseCursor {
 public:
  explicit MouseCursor(const boost::shared_ptr<const Surface>& cursor_surface);
  ~MouseCursor();

  // Renders the cursor to the screen, taking the hotspot offset into account.
  void renderHotspotAt(const Point& mouse_pt);

  // Returns (renderX, renderY) which is the upper left corner of where the
  // cursor is to be rendered for the incoming mouse location (mouseX, mouseY).
  Point getTopLeftForHotspotAt(const Point& mouse_location);

 private:
  // Sets hotspot_[XY] to the white pixel in the
  void findHotspot();

  // The raw image read from the PDT.
  boost::shared_ptr<const Surface> cursor_surface_;

  // The hotspot location.
  Size hotspot_offset_;
};  // end of class MouseCursor

#endif  // SRC_SYSTEMS_BASE_MOUSECURSOR_HPP_
