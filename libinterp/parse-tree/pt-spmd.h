////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2020-2024 The Octave Project Developers
//
// See the file COPYRIGHT.md in the top-level directory of this
// distribution or <https://octave.org/copyright/>.
//
// This file is part of Octave.
//
// Octave is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Octave is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Octave; see the file COPYING.  If not, see
// <https://www.gnu.org/licenses/>.
//
////////////////////////////////////////////////////////////////////////

#if ! defined (octave_pt_spmd_command_h)
#define octave_pt_spmd_command_h 1

#include "octave-config.h"

#include "pt-cmd.h"
#include "pt-walk.h"

OCTAVE_BEGIN_NAMESPACE(octave)

class comment_list;
class tree_statement_list;

// Spmd.

class tree_spmd_command : public tree_command
{
public:

  tree_spmd_command (tree_statement_list *body, int l = -1, int c = -1)
    : tree_command (l, c), m_body (body)
  { }

  OCTAVE_DISABLE_CONSTRUCT_COPY_MOVE (tree_spmd_command)

  ~tree_spmd_command ();

  tree_statement_list * body () { return m_body; }

  void accept (tree_walker& tw)
  {
    tw.visit_spmd_command (*this);
  }

private:

  // List of commands.
  tree_statement_list *m_body;
};

OCTAVE_END_NAMESPACE(octave)

#endif
