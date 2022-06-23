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






