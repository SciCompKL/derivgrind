! -------------------------------------------------------------------- !
! --- Wrap client request            derivgrind_clientrequests.f90 --- !
! --- functions for Fortran.                                       --- !
! -------------------------------------------------------------------- !

!
!  This file is part of DerivGrind, a tool performing forward-mode
!  algorithmic differentiation of compiled programs, implemented
!  in the Valgrind framework.
!
!  Copyright (C) 2022 Chair for Scientific Computing (SciComp), TU Kaiserslautern
!  Homepage: https://www.scicomp.uni-kl.de
!  Contact: Prof. Nicolas R. Gauger (derivgrind@scicomp.uni-kl.de)
!
!  Lead developer: Max Aehle (SciComp, TU Kaiserslautern)
!
!  This program is free software; you can redistribute it and/or
!  modify it under the terms of the GNU General Public License as
!  published by the Free Software Foundation; either version 2 of the
!  License, or (at your option) any later version.
!
!  This program is distributed in the hope that it will be useful, but
!  WITHOUT ANY WARRANTY; without even the implied warranty of
!  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
!  General Public License for more details.
!
!  You should have received a copy of the GNU General Public License
!  along with this program; if not, see <http://www.gnu.org/licenses/>.
!
!  The GNU General Public License is contained in the file COPYING.
!

module derivgrind_clientrequests
  use, intrinsic :: iso_c_binding
  implicit none

  interface
    subroutine valgrind_set_derivative(val, grad, size_) bind(C)
      use, intrinsic :: iso_c_binding
      implicit none
      type(c_ptr)  :: val
      type(c_ptr)  :: grad
      integer(kind=c_int), intent(in) :: size_
    end subroutine valgrind_set_derivative
  end interface
  interface
    subroutine valgrind_get_derivative(val, grad, size_) bind(C)
      use, intrinsic :: iso_c_binding
      implicit none
      type(c_ptr)  :: val
      type(c_ptr)  :: grad
      integer(kind=c_int), intent(in) :: size_
    end subroutine valgrind_get_derivative
  end interface
end module






