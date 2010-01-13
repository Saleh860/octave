/*

Copyright (C) 1996, 1997, 1999, 2000, 2003, 2005, 2006, 2007, 2008, 2009
              John W. Eaton

This file is part of Octave.

Octave is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 3 of the License, or (at your
option) any later version.

Octave is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with Octave; see the file COPYING.  If not, see
<http://www.gnu.org/licenses/>.

*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "CmplxLU.h"
#include "dbleLU.h"
#include "fCmplxLU.h"
#include "floatLU.h"
#include "SparseCmplxLU.h"
#include "SparsedbleLU.h"

#include "defun-dld.h"
#include "error.h"
#include "gripes.h"
#include "oct-obj.h"
#include "utils.h"
#include "ov-re-sparse.h"
#include "ov-cx-sparse.h"

template <class MT>
static octave_value
get_lu_l (const base_lu<MT>& fact)
{
  MT L = fact.L ();
  if (L.is_square ())
    return octave_value (L, MatrixType (MatrixType::Lower));
  else
    return L;
}

template <class MT>
static octave_value
get_lu_u (const base_lu<MT>& fact)
{
  MT U = fact.U ();
  if (U.is_square () && fact.regular ())
    return octave_value (U, MatrixType (MatrixType::Upper));
  else
    return U;
}

DEFUN_DLD (lu, args, nargout,
  "-*- texinfo -*-\n\
@deftypefn {Loadable Function} {[@var{l}, @var{u}, @var{p}] =} lu (@var{a})\n\
@deftypefnx {Loadable Function} {[@var{l}, @var{u}, @var{p}, @var{q}] =} lu (@var{s})\n\
@deftypefnx {Loadable Function} {[@var{l}, @var{u}, @var{p}, @var{q}, @var{r}] =} lu (@var{s})\n\
@deftypefnx {Loadable Function} {[@dots{}] =} lu (@var{s}, @var{thres})\n\
@deftypefnx {Loadable Function} {@var{y} =} lu (@dots{})\n\
@deftypefnx {Loadable Function} {[@dots{}] =} lu (@dots{}, 'vector')\n\
@cindex LU decomposition\n\
Compute the LU decomposition of @var{a}.  If @var{a} is full subroutines from\n\
@sc{lapack} are used and if @var{a} is sparse then UMFPACK is used.  The\n\
result is returned in a permuted form, according to the optional return\n\
value @var{p}.  For example, given the matrix @code{a = [1, 2; 3, 4]},\n\
\n\
@example\n\
[l, u, p] = lu (a)\n\
@end example\n\
\n\
@noindent\n\
returns\n\
\n\
@example\n\
@group\n\
l =\n\
\n\
  1.00000  0.00000\n\
  0.33333  1.00000\n\
\n\
u =\n\
\n\
  3.00000  4.00000\n\
  0.00000  0.66667\n\
\n\
p =\n\
\n\
  0  1\n\
  1  0\n\
@end group\n\
@end example\n\
\n\
The matrix is not required to be square.\n\
\n\
Called with two or three output arguments and a spare input matrix,\n\
then @dfn{lu} does not attempt to perform sparsity preserving column\n\
permutations.  Called with a fourth output argument, the sparsity\n\
preserving column transformation @var{Q} is returned, such that\n\
@code{@var{p} * @var{a} * @var{q} = @var{l} * @var{u}}.\n\
\n\
Called with a fifth output argument and a sparse input matrix, then\n\
@dfn{lu} attempts to use a scaling factor @var{r} on the input matrix\n\
such that @code{@var{p} * (@var{r} \\ @var{a}) * @var{q} = @var{l} * @var{u}}.\n\
This typically leads to a sparser and more stable factorization.\n\
\n\
An additional input argument @var{thres}, that defines the pivoting\n\
threshold can be given.  @var{thres} can be a scalar, in which case\n\
it defines UMFPACK pivoting tolerance for both symmetric and unsymmetric\n\
cases.  If @var{thres} is a two element vector, then the first element\n\
defines the pivoting tolerance for the unsymmetric UMFPACK pivoting\n\
strategy and the second the symmetric strategy.  By default, the values\n\
defined by @code{spparms} are used and are by default @code{[0.1, 0.001]}.\n\
\n\
Given the string argument 'vector', @dfn{lu} returns the values of @var{p}\n\
@var{q} as vector values, such that for full matrix, @code{@var{a}\n\
(@var{p},:) = @var{l} * @var{u}}, and @code{@var{r}(@var{p},:) * @var{a}\n\
(:, @var{q}) = @var{l} * @var{u}}.\n\
\n\
With two output arguments, returns the permuted forms of the upper and\n\
lower triangular matrices, such that @code{@var{a} = @var{l} * @var{u}}.\n\
With one output argument @var{y}, then the matrix returned by the @sc{lapack}\n\
routines is returned.  If the input matrix is sparse then the matrix @var{l}\n\
is embedded into @var{u} to give a return value similar to the full case.\n\
For both full and sparse matrices, @dfn{lu} looses the permutation\n\
information.\n\
@end deftypefn")
{
  octave_value_list retval;
  int nargin = args.length ();
  bool issparse = (nargin > 0 && args(0).is_sparse_type ());
  bool scale = (nargout  == 5);

  if (nargin < 1 || (issparse && (nargin > 3 || nargout > 5)) 
      || (!issparse && (nargin > 2 || nargout > 3)))
    {
      print_usage ();
      return retval;
    }

  bool vecout = false;
  Matrix thres;

  int n = 1;
  while (n < nargin && ! error_state)
    {
      if (args (n).is_string ())
	{
	  std::string tmp = args(n++).string_value ();

	  if (! error_state )
	    {
	      if (tmp.compare ("vector") == 0)
		vecout = true;
	      else
		error ("lu: unrecognized string argument");
	    }
	}
      else
	{
	  Matrix tmp = args(n++).matrix_value ();

	  if (! error_state )
	    {
	      if (!issparse)
		error ("lu: can not define pivoting threshold for full matrices");
	      else if (tmp.nelem () == 1)
		{
		  thres.resize(1,2);
		  thres(0) = tmp(0);
		  thres(1) = tmp(0);
		}
	      else if (tmp.nelem () == 2)
		thres = tmp;
	      else
		error ("lu: expecting 2 element vector for thres");
	    }
	}
    }

  octave_value arg = args(0);

  octave_idx_type nr = arg.rows ();
  octave_idx_type nc = arg.columns ();

  int arg_is_empty = empty_arg ("lu", nr, nc);

  if (issparse)
    {
      if (arg_is_empty < 0)
	return retval;
      else if (arg_is_empty > 0)
	return octave_value_list (5, SparseMatrix ());

      ColumnVector Qinit;
      if (nargout < 4)
	{
	  Qinit.resize (nc);
	  for (octave_idx_type i = 0; i < nc; i++)
	    Qinit (i) = i;
	}

      if (arg.is_real_type ())
	{
	  SparseMatrix m = arg.sparse_matrix_value ();

	  switch (nargout)
	    {
	    case 0:
	    case 1:
	    case 2:
	      {
		SparseLU fact (m, Qinit, thres, false, true);

		if (nargout < 2)
		  retval (0) = fact.Y ();
		else
		  {
		    PermMatrix P = fact.Pr_mat ();
		    SparseMatrix L = P.transpose () * fact.L ();
		    retval(1) = octave_value (fact.U (), 
					      MatrixType (MatrixType::Upper));

		    retval(0) = octave_value (L, 
			MatrixType (MatrixType::Permuted_Lower, 
				    nr, fact.row_perm ()));
		  }
	      }
	      break;

	    case 3:
	      {
		SparseLU fact (m, Qinit, thres, false, true);

		if (vecout)
		  retval (2) = fact.Pr_vec ();
		else
		  retval(2) = fact.Pr_mat ();

		retval(1) = octave_value (fact.U (), 
					  MatrixType (MatrixType::Upper));
		retval(0) = octave_value (fact.L (), 
					  MatrixType (MatrixType::Lower));
	      }
	      break;

	    case 4:
	    default:
	      {
		SparseLU fact (m, thres, scale);

		if (scale)
		  retval(4) = fact.R ();

		if (vecout)
		  {
		    retval(3) = fact.Pc_vec ();
		    retval(2) = fact.Pr_vec ();
		  }
		else
		  {
		    retval(3) = fact.Pc_mat ();
		    retval(2) = fact.Pr_mat ();
		  }
		retval(1) = octave_value (fact.U (), 
					  MatrixType (MatrixType::Upper));
		retval(0) = octave_value (fact.L (), 
					  MatrixType (MatrixType::Lower));
	      }
	      break;
	    }
	}
      else if (arg.is_complex_type ())
	{
	  SparseComplexMatrix m = arg.sparse_complex_matrix_value ();

	  switch (nargout)
	    {
	    case 0:
	    case 1:
	    case 2:
	      {
		SparseComplexLU fact (m, Qinit, thres, false, true);

		if (nargout < 2)
		  retval (0) = fact.Y ();
		else
		  {
		    PermMatrix P = fact.Pr_mat ();
		    SparseComplexMatrix L = P.transpose () * fact.L ();
		    retval(1) = octave_value (fact.U (), 
					      MatrixType (MatrixType::Upper));

		    retval(0) = octave_value (L, 
			MatrixType (MatrixType::Permuted_Lower, 
				    nr, fact.row_perm ()));
		  }
	      }
	      break;

	    case 3:
	      {
		SparseComplexLU fact (m, Qinit, thres, false, true);

		if (vecout)
		  retval (2) = fact.Pr_vec ();
		else
		  retval(2) = fact.Pr_mat ();

		retval(1) = octave_value (fact.U (), 
					  MatrixType (MatrixType::Upper));
		retval(0) = octave_value (fact.L (), 
					  MatrixType (MatrixType::Lower));
	      }
	      break;

	    case 4:
	    default:
	      {
		SparseComplexLU fact (m, thres, scale);

		if (scale)
		  retval(4) = fact.R ();

		if (vecout)
		  {
		    retval(3) = fact.Pc_vec ();
		    retval(2) = fact.Pr_vec ();
		  }
		else
		  {
		    retval(3) = fact.Pc_mat ();
		    retval(2) = fact.Pr_mat ();
		  }
		retval(1) = octave_value (fact.U (), 
					  MatrixType (MatrixType::Upper));
		retval(0) = octave_value (fact.L (), 
					  MatrixType (MatrixType::Lower));
	      }
	      break;
	    }
	}
      else
	gripe_wrong_type_arg ("lu", arg);
    }
  else
    {
      if (arg_is_empty < 0)
	return retval;
      else if (arg_is_empty > 0)
	return octave_value_list (3, Matrix ());

      if (arg.is_real_type ())
	{
	  if (arg.is_single_type ())
	    {
	      FloatMatrix m = arg.float_matrix_value ();

	      if (! error_state)
		{
		  FloatLU fact (m);

		  switch (nargout)
		    {
		    case 0:
		    case 1:
		      retval(0) = fact.Y ();
		      break;

		    case 2:
		      {
			PermMatrix P = fact.P ();
			FloatMatrix L = P.transpose () * fact.L ();
			retval(1) = get_lu_u (fact);
			retval(0) = L;
		      }
		      break;

		    case 3:
		    default:
		      {
			if (vecout)
			  retval(2) = fact.P_vec ();
			else
			  retval(2) = fact.P ();
			retval(1) = get_lu_u (fact);
			retval(0) = get_lu_l (fact);
		      }
		      break;
		    }
		}
	    }
	  else
	    {
	      Matrix m = arg.matrix_value ();

	      if (! error_state)
		{
		  LU fact (m);

		  switch (nargout)
		    {
		    case 0:
		    case 1:
		      retval(0) = fact.Y ();
		      break;

		    case 2:
		      {
			PermMatrix P = fact.P ();
			Matrix L = P.transpose () * fact.L ();
			retval(1) = get_lu_u (fact);
			retval(0) = L;
		      }
		      break;

		    case 3:
		    default:
		      {
			if (vecout)
			  retval(2) = fact.P_vec ();
			else
			  retval(2) = fact.P ();
			retval(1) = get_lu_u (fact);
			retval(0) = get_lu_l (fact);
		      }
		      break;
		    }
		}
	    }
	}
      else if (arg.is_complex_type ())
	{
	  if (arg.is_single_type ())
	    {
	      FloatComplexMatrix m = arg.float_complex_matrix_value ();

	      if (! error_state)
		{
		  FloatComplexLU fact (m);

		  switch (nargout)
		    {
		    case 0:
		    case 1:
		      retval(0) = fact.Y ();
		      break;

		    case 2:
		      {
			PermMatrix P = fact.P ();
			FloatComplexMatrix L = P.transpose () * fact.L ();
			retval(1) = get_lu_u (fact);
			retval(0) = L;
		      }
		      break;

		    case 3:
		    default:
		      {
			if (vecout)
			  retval(2) = fact.P_vec ();
			else
			  retval(2) = fact.P ();
			retval(1) = get_lu_u (fact);
			retval(0) = get_lu_l (fact);
		      }
		      break;
		    }
		}
	    }
	  else
	    {
	      ComplexMatrix m = arg.complex_matrix_value ();

	      if (! error_state)
		{
		  ComplexLU fact (m);

		  switch (nargout)
		    {
		    case 0:
		    case 1:
		      retval(0) = fact.Y ();
		      break;

		    case 2:
		      {
			PermMatrix P = fact.P ();
			ComplexMatrix L = P.transpose () * fact.L ();
			retval(1) = get_lu_u (fact);
			retval(0) = L;
		      }
		      break;

		    case 3:
		    default:
		      {
			if (vecout)
			  retval(2) = fact.P_vec ();
			else
			  retval(2) = fact.P ();
			retval(1) = get_lu_u (fact);
			retval(0) = get_lu_l (fact);
		      }
		      break;
		    }
		}
	    }
	}
      else
	gripe_wrong_type_arg ("lu", arg);
    }

  return retval;
}

/*

%!assert(lu ([1, 2; 3, 4]), [3, 4; 1/3, 2/3], eps);

%!test
%! [l, u] = lu ([1, 2; 3, 4]);
%! assert(l, [1/3, 1; 1, 0], sqrt (eps));
%! assert(u, [3, 4; 0, 2/3], sqrt (eps));

%!test
%! [l, u, p] = lu ([1, 2; 3, 4]);
%! assert(l, [1, 0; 1/3, 1], sqrt (eps));
%! assert(u, [3, 4; 0, 2/3], sqrt (eps));
%! assert(p(:,:), [0, 1; 1, 0], sqrt (eps));

%!test
%! [l, u, p] = lu ([1, 2; 3, 4],'vector');
%! assert(l, [1, 0; 1/3, 1], sqrt (eps));
%! assert(u, [3, 4; 0, 2/3], sqrt (eps));
%! assert(p, [2;1], sqrt (eps));

%!test
%! [l u p] = lu ([1, 2; 3, 4; 5, 6]);
%! assert(l, [1, 0; 1/5, 1; 3/5, 1/2], sqrt (eps));
%! assert(u, [5, 6; 0, 4/5], sqrt (eps));
%! assert(p(:,:), [0, 0, 1; 1, 0, 0; 0 1 0], sqrt (eps));

%!assert(lu (single([1, 2; 3, 4])), single([3, 4; 1/3, 2/3]), eps('single'));

%!test
%! [l, u] = lu (single([1, 2; 3, 4]));
%! assert(l, single([1/3, 1; 1, 0]), sqrt (eps('single')));
%! assert(u, single([3, 4; 0, 2/3]), sqrt (eps('single')));

%!test
%! [l, u, p] = lu (single([1, 2; 3, 4]));
%! assert(l, single([1, 0; 1/3, 1]), sqrt (eps('single')));
%! assert(u, single([3, 4; 0, 2/3]), sqrt (eps('single')));
%! assert(p(:,:), single([0, 1; 1, 0]), sqrt (eps('single')));

%!test
%! [l, u, p] = lu (single([1, 2; 3, 4]),'vector');
%! assert(l, single([1, 0; 1/3, 1]), sqrt (eps('single')));
%! assert(u, single([3, 4; 0, 2/3]), sqrt (eps('single')));
%! assert(p, single([2;1]), sqrt (eps('single')));

%!test
%! [l u p] = lu (single([1, 2; 3, 4; 5, 6]));
%! assert(l, single([1, 0; 1/5, 1; 3/5, 1/2]), sqrt (eps('single')));
%! assert(u, single([5, 6; 0, 4/5]), sqrt (eps('single')));
%! assert(p(:,:), single([0, 0, 1; 1, 0, 0; 0 1 0]), sqrt (eps('single')));

%!error <Invalid call to lu.*> lu ();
%!error lu ([1, 2; 3, 4], 2);

 */

static
bool check_lu_dims (const octave_value& l, const octave_value& u,
                    const octave_value& p)
{
  octave_idx_type m = l.rows (), k = u.rows (), n = u.columns ();
  return ((l.ndims () == 2 && u.ndims () == 2 && k == l.columns ())
            && k == std::min (m, n) &&
            (p.is_undefined () || p.rows () == m));
}

DEFUN_DLD (luupdate, args, ,
  "-*- texinfo -*-\n\
@deftypefn  {Loadable Function} {[@var{L}, @var{U}] =} luupdate (@var{l}, @var{u}, @var{x}, @var{y})\n\
@deftypefnx {Loadable Function} {[@var{L}, @var{U}, @var{P}] =} luupdate (@var{L}, @var{U}, @var{P}, @var{x}, @var{y})\n\
Given an LU@tie{}factorization of a real or complex matrix\n\
@w{@var{A} = @var{L}*@var{U}}, @var{L}@tie{}lower unit trapezoidal and\n\
@var{U}@tie{}upper trapezoidal, return the LU@tie{}factorization\n\
of @w{@var{A} + @var{x}*@var{y}.'}, where @var{x} and @var{y} are\n\
column vectors (rank-1 update) or matrices with equal number of columns\n\
(rank-k update).\n\
Optionally, row-pivoted updating can be used by supplying\n\
a row permutation (pivoting) matrix @var{P};\n\
in that case, an updated permutation matrix is returned.\n\
Note that if @var{L}, @var{U}, @var{P} is a pivoted LU@tie{}factorization\n\
as obtained by @code{lu}:\n\
\n\
@example\n\
  [@var{L}, @var{U}, @var{P}] = lu (@var{A});\n\
@end example\n\
\n\
then a factorization of @code{@var{a}+@var{x}*@var{y}.'} can be obtained either as\n\
\n\
@example\n\
  [@var{L1}, @var{U1}] = lu (@var{L}, @var{U}, @var{P}*@var{x}, @var{y})\n\
@end example\n\
\n\
or\n\
\n\
@example\n\
  [@var{L1}, @var{U1}, @var{P1}] = lu (@var{L}, @var{U}, @var{P}, @var{x}, @var{y})\n\
@end example\n\
\n\
The first form uses the unpivoted algorithm, which is faster, but less stable.\n\
The second form uses a slower pivoted algorithm, which is more stable.\n\
\n\
Note that the matrix case is done as a sequence of rank-1 updates;\n\
thus, for k large enough, it will be both faster and more accurate to recompute\n\
the factorization from scratch.\n\
@seealso{lu,qrupdate,cholupdate}\n\
@end deftypefn")
{
  octave_idx_type nargin = args.length ();
  octave_value_list retval;

  bool pivoted = nargin == 5;

  if (nargin != 4 && nargin != 5)
    {
      print_usage ();
      return retval;
    }

  octave_value argl = args(0);
  octave_value argu = args(1);
  octave_value argp = pivoted ? args(2) : octave_value ();
  octave_value argx = args(2 + pivoted);
  octave_value argy = args(3 + pivoted);

  if (argl.is_numeric_type () && argu.is_numeric_type () 
      && argx.is_numeric_type () && argy.is_numeric_type ()
      && (! pivoted || argp.is_perm_matrix ()))
    {
      if (check_lu_dims (argl, argu, argp))
        {
          PermMatrix P = (pivoted 
                          ? argp.perm_matrix_value () 
                          : PermMatrix::eye (argl.rows ()));

          if (argl.is_real_type () 
	      && argu.is_real_type () 
	      && argx.is_real_type () 
	      && argy.is_real_type ())
            {
	      // all real case
	      if (argl.is_single_type () 
		  || argu.is_single_type () 
		  || argx.is_single_type () 
		  || argy.is_single_type ())
		{
		  FloatMatrix L = argl.float_matrix_value ();
		  FloatMatrix U = argu.float_matrix_value ();
		  FloatMatrix x = argx.float_matrix_value ();
		  FloatMatrix y = argy.float_matrix_value ();

		  FloatLU fact (L, U, P);
                  if (pivoted)
                    fact.update_piv (x, y);
                  else
                    fact.update (x, y);

                  if (pivoted)
                    retval(2) = fact.P ();
		  retval(1) = get_lu_u (fact);
		  retval(0) = get_lu_l (fact);
		}
	      else
		{
		  Matrix L = argl.matrix_value ();
		  Matrix U = argu.matrix_value ();
		  Matrix x = argx.matrix_value ();
		  Matrix y = argy.matrix_value ();

		  LU fact (L, U, P);
                  if (pivoted)
                    fact.update_piv (x, y);
                  else
                    fact.update (x, y);

                  if (pivoted)
                    retval(2) = fact.P ();
		  retval(1) = get_lu_u (fact);
		  retval(0) = get_lu_l (fact);
		}
            }
          else
            {
              // complex case
	      if (argl.is_single_type () 
		  || argu.is_single_type () 
		  || argx.is_single_type () 
		  || argy.is_single_type ())
		{
		  FloatComplexMatrix L = argl.float_complex_matrix_value ();
		  FloatComplexMatrix U = argu.float_complex_matrix_value ();
		  FloatComplexMatrix x = argx.float_complex_matrix_value ();
		  FloatComplexMatrix y = argy.float_complex_matrix_value ();

		  FloatComplexLU fact (L, U, P);
                  if (pivoted)
                    fact.update_piv (x, y);
                  else
                    fact.update (x, y);
              
                  if (pivoted)
                    retval(2) = fact.P ();
		  retval(1) = get_lu_u (fact);
		  retval(0) = get_lu_l (fact);
		}
	      else
		{
		  ComplexMatrix L = argl.complex_matrix_value ();
		  ComplexMatrix U = argu.complex_matrix_value ();
		  ComplexMatrix x = argx.complex_matrix_value ();
		  ComplexMatrix y = argy.complex_matrix_value ();

		  ComplexLU fact (L, U, P);
                  if (pivoted)
                    fact.update_piv (x, y);
                  else
                    fact.update (x, y);
              
                  if (pivoted)
                    retval(2) = fact.P ();
		  retval(1) = get_lu_u (fact);
		  retval(0) = get_lu_l (fact);
		}
            }
        }
      else
	error ("luupdate: dimensions mismatch");
    }
  else
    error ("luupdate: expecting numeric arguments");

  return retval;
}

/*
%!shared A, u, v, Ac, uc, vc
%! A = [0.091364  0.613038  0.999083;
%!      0.594638  0.425302  0.603537;
%!      0.383594  0.291238  0.085574;
%!      0.265712  0.268003  0.238409;
%!      0.669966  0.743851  0.445057 ];
%!
%! u = [0.85082;  
%!      0.76426;  
%!      0.42883;  
%!      0.53010;  
%!      0.80683 ];
%!
%! v = [0.98810;
%!      0.24295;
%!      0.43167 ];
%!
%! Ac = [0.620405 + 0.956953i  0.480013 + 0.048806i  0.402627 + 0.338171i;
%!      0.589077 + 0.658457i  0.013205 + 0.279323i  0.229284 + 0.721929i;
%!      0.092758 + 0.345687i  0.928679 + 0.241052i  0.764536 + 0.832406i;
%!      0.912098 + 0.721024i  0.049018 + 0.269452i  0.730029 + 0.796517i;
%!      0.112849 + 0.603871i  0.486352 + 0.142337i  0.355646 + 0.151496i ];
%!
%! uc = [0.20351 + 0.05401i;
%!      0.13141 + 0.43708i;
%!      0.29808 + 0.08789i;
%!      0.69821 + 0.38844i;
%!      0.74871 + 0.25821i ];
%!
%! vc = [0.85839 + 0.29468i;
%!      0.20820 + 0.93090i;
%!      0.86184 + 0.34689i ];
%!

%!testif HAVE_QRUPDATE_LUU
%! [L,U,P] = lu(A);
%! [L,U] = luupdate(L,U,P*u,v);
%! assert(norm(vec(tril(L)-L),Inf) == 0)
%! assert(norm(vec(triu(U)-U),Inf) == 0)
%! assert(norm(vec(P'*L*U - A - u*v.'),Inf) < norm(A)*1e1*eps)
%! 
%!testif HAVE_QRUPDATE_LUU
%! [L,U,P] = lu(Ac);
%! [L,U] = luupdate(L,U,P*uc,vc);
%! assert(norm(vec(tril(L)-L),Inf) == 0)
%! assert(norm(vec(triu(U)-U),Inf) == 0)
%! assert(norm(vec(P'*L*U - Ac - uc*vc.'),Inf) < norm(Ac)*1e1*eps)

%!testif HAVE_QRUPDATE_LUU
%! [L,U,P] = lu(single(A));
%! [L,U] = luupdate(L,U,P*single(u),single(v));
%! assert(norm(vec(tril(L)-L),Inf) == 0)
%! assert(norm(vec(triu(U)-U),Inf) == 0)
%! assert(norm(vec(P'*L*U - single(A) - single(u)*single(v).'),Inf) < norm(single(A))*1e1*eps('single'))
%! 
%!testif HAVE_QRUPDATE_LUU
%! [L,U,P] = lu(single(Ac));
%! [L,U] = luupdate(L,U,P*single(uc),single(vc));
%! assert(norm(vec(tril(L)-L),Inf) == 0)
%! assert(norm(vec(triu(U)-U),Inf) == 0)
%! assert(norm(vec(P'*L*U - single(Ac) - single(uc)*single(vc).'),Inf) < norm(single(Ac))*1e1*eps('single'))

%!testif HAVE_QRUPDATE_LUU
%! [L,U,P] = lu(A);
%! [L,U,P] = luupdate(L,U,P,u,v);
%! assert(norm(vec(tril(L)-L),Inf) == 0)
%! assert(norm(vec(triu(U)-U),Inf) == 0)
%! assert(norm(vec(P'*L*U - A - u*v.'),Inf) < norm(A)*1e1*eps)
%! 
%!testif HAVE_QRUPDATE_LUU
%! [L,U,P] = lu(Ac);
%! [L,U,P] = luupdate(L,U,P,uc,vc);
%! assert(norm(vec(tril(L)-L),Inf) == 0)
%! assert(norm(vec(triu(U)-U),Inf) == 0)
%! assert(norm(vec(P'*L*U - Ac - uc*vc.'),Inf) < norm(Ac)*1e1*eps)

%!testif HAVE_QRUPDATE_LUU
%! [L,U,P] = lu(single(A));
%! [L,U,P] = luupdate(L,U,P,single(u),single(v));
%! assert(norm(vec(tril(L)-L),Inf) == 0)
%! assert(norm(vec(triu(U)-U),Inf) == 0)
%! assert(norm(vec(P'*L*U - single(A) - single(u)*single(v).'),Inf) < norm(single(A))*1e1*eps('single'))
%! 
%!testif HAVE_QRUPDATE_LUU
%! [L,U,P] = lu(single(Ac));
%! [L,U,P] = luupdate(L,U,P,single(uc),single(vc));
%! assert(norm(vec(tril(L)-L),Inf) == 0)
%! assert(norm(vec(triu(U)-U),Inf) == 0)
%! assert(norm(vec(P'*L*U - single(Ac) - single(uc)*single(vc).'),Inf) < norm(single(Ac))*1e1*eps('single'))
*/

/*
;;; Local Variables: ***
;;; mode: C++ ***
;;; End: ***
*/
