########################################################################
##
## Copyright (C) 2023 The Octave Project Developers
##
## See the file COPYRIGHT.md in the top-level directory of this
## distribution or <https://octave.org/copyright/>.
##
## This file is part of Octave.
##
## Octave is free software: you can redistribute it and/or modify it
## under the terms of the GNU General Public License as published by
## the Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
##
## Octave is distributed in the hope that it will be useful, but
## WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with Octave; see the file COPYING.  If not, see
## <https://www.gnu.org/licenses/>.
##
########################################################################

## -*- texinfo -*-
## @deftypefn {} {} vm ()
## Summary of commands related to Octave's virtual machine (VM) evaluator.
##
## For more information on each command and available options use
## @code{help CMD}.
##
## The VM commands available in Octave are:
##
## @table @code
## @item vm_enable
## Main VM user function.  Switch on or off the VM as a whole.
##
## @item vm_compile
## VM user function.  Compile a specified function to bytecode.
##
## @item vm_clear_cache
## Internal function.  Clear the cache of already-processed code.
##
## @item vm_is_compiled
## VM internal function.  Return true if the specified function is compiled.
##
## @item vm_is_executing
## Internal function.  Return true if the VM is executing.
##
## @item vm_print_bytecode
## Internal function.  Print bytecode of specified function.
##
## @item vm_print_trace
## Internal function.  Print a debug trace from the VM.
##
## @item vm_profile
## Internal function.  Profile the code running in the VM.
##
## @end table
##
## @noindent
##
## @seealso{vm_enable, vm_compile, vm_clear_cache,
## vm_is_compiled, vm_is_executing, vm_print_bytecode,
## vm_print_trace, vm_profile}
## @end deftypefn

function vm ()
  help ("vm");
endfunction

## Mark file as being tested.  No real test needed for a documentation .m file
%!assert (1)
