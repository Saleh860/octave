////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2008-2024 The Octave Project Developers
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

#if ! defined (octave_pt_cbinop_h)
#define octave_pt_cbinop_h 1

#include "octave-config.h"

#include <string>

class octave_value;
class octave_value_list;

#include "ov.h"
#include "pt-binop.h"
#include "pt-walk.h"

OCTAVE_BEGIN_NAMESPACE(octave)

// Binary expressions that can be reduced to compound operations

class tree_compound_binary_expression : public tree_binary_expression
{
public:

  tree_compound_binary_expression (tree_expression *a, const token& op_tok, tree_expression *b, octave_value::binary_op t,
                                   tree_expression *ca, tree_expression *cb, octave_value::compound_binary_op ct)
    : tree_binary_expression (a, op_tok, b, t), m_lhs (ca), m_rhs (cb), m_etype (ct)
  { }

  OCTAVE_DISABLE_CONSTRUCT_COPY_MOVE (tree_compound_binary_expression)

  // FIXME: who is responsibile for deleting M_LHS and M_RHS?
  ~tree_compound_binary_expression () = default;

  octave_value::compound_binary_op cop_type () const { return m_etype; }

  bool rvalue_ok () const { return true; }

  tree_expression * clhs () { return m_lhs; }
  tree_expression * crhs () { return m_rhs; }

  octave_value evaluate (tree_evaluator&, int nargout = 1);

  octave_value_list evaluate_n (tree_evaluator& tw, int nargout = 1)
  {
    return ovl (evaluate (tw, nargout));
  }

  void accept (tree_walker& tw)
  {
    tw.visit_compound_binary_expression (*this);
  }

private:

  tree_expression *m_lhs;
  tree_expression *m_rhs;

  octave_value::compound_binary_op m_etype;
};

// a "virtual constructor"

tree_binary_expression *
maybe_compound_binary_expression (tree_expression *a, const token& op_tok, tree_expression *b, octave_value::binary_op t = octave_value::unknown_binary_op);

OCTAVE_END_NAMESPACE(octave)

#endif
